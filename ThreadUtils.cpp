#include "ThreadUtils.h"

namespace fre {

    using namespace std;

    ThreadPool2::ThreadPool2(size_t worker_count)
    {
        if (worker_count == 0) worker_count = 1;
        window_start_ = chrono::steady_clock::now();

        workers_.reserve(worker_count);
        for (size_t i = 0; i < worker_count; ++i) {
            workers_.emplace_back([this]() { worker_loop(); });
        }
    }

    ThreadPool2::~ThreadPool2()
    {
        stop_gracefully();
    }

    void ThreadPool2::worker_loop()
    {
        while (true) {
            function<void()> task;

            {
                unique_lock<mutex> lock(mtx_);
                cv_.wait(lock, [this]() {
                    return stopping_ || !q_.empty();
                });

                if (stopping_ && q_.empty()) {
                    return;
                }

                task = std::move(q_.front());
                q_.pop();
                ++inflight_;
            }

            // Execute outside the lock
            task();

            {
                lock_guard<mutex> lock(mtx_);
                --inflight_;
                if (q_.empty() && inflight_.load() == 0) {
                    drained_cv_.notify_all();
                }
            }
        }
    }

    void ThreadPool2::drain()
    {
        unique_lock<mutex> lock(mtx_);
        drained_cv_.wait(lock, [this]() {
            return q_.empty() && inflight_.load() == 0;
        });
    }

    void ThreadPool2::stop_gracefully()
    {
        {
            lock_guard<mutex> lock(mtx_);
            if (stopping_) return;
            accepting_ = false;
        }

        // Wait for completion of queued work
        drain();

        {
            lock_guard<mutex> lock(mtx_);
            stopping_ = true;
        }
        cv_.notify_all();

        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
        workers_.clear();
    }

    void ThreadPool2::stop_now()
    {
        {
            lock_guard<mutex> lock(mtx_);
            if (stopping_) return;
            accepting_ = false;

            // Clear queued tasks immediately
            while (!q_.empty()) q_.pop();

            stopping_ = true;
        }
        cv_.notify_all();

        for (auto& t : workers_) {
            if (t.joinable()) t.join();
        }
        workers_.clear();
    }

    size_t ThreadPool2::pending() const
    {
        lock_guard<mutex> lock(mtx_);
        return q_.size();
    }

    int ThreadPool2::in_flight() const
    {
        return inflight_.load();
    }

    void ThreadPool2::set_qps_limit(int qps)
    {
        lock_guard<mutex> lock(qps_mtx_);
        qps_ = (qps < 0 ? 0 : qps);
        window_start_ = chrono::steady_clock::now();
        used_in_window_ = 0;
    }

    void ThreadPool2::acquire_permit()
    {
        while (true) {
            int local_qps = 0;
            chrono::steady_clock::time_point local_start;

            {
                lock_guard<mutex> lock(qps_mtx_);
                local_qps = qps_;
                local_start = window_start_;

                if (local_qps == 0) {
                    return; // unlimited
                }

                auto now = chrono::steady_clock::now();
                auto elapsed_ms =
                    chrono::duration_cast<chrono::milliseconds>(now - window_start_).count();

                // Use 1-second fixed window
                if (elapsed_ms >= 1000) {
                    window_start_ = now;
                    used_in_window_ = 0;
                }

                if (used_in_window_ < local_qps) {
                    ++used_in_window_;
                    return;
                }
            }

            // Not allowed yet; sleep a bit and retry
            this_thread::sleep_for(chrono::milliseconds(5));
        }
    }

} // namespace fre
