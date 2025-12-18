#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <future>
#include <type_traits>

namespace fre {

    class ThreadPool2 {
    public:
        // Start worker threads immediately
        ThreadPool2(size_t worker_count = std::thread::hardware_concurrency());

        // Stop workers and join
        ~ThreadPool2();

        // Submit a task and get a future for its result
        template <class F, class... Args>
        auto submit(F&& f, Args&&... args)
            -> std::future<std::invoke_result_t<F, Args...>>;

        // Block until all queued tasks are finished
        void drain();

        // Stop accepting new tasks; finish queued tasks then stop workers
        void stop_gracefully();

        // Stop immediately: clear queue and stop workers ASAP
        void stop_now();

        // Queue size (tasks not yet started)
        size_t pending() const;

        // Number of tasks currently executing
        int in_flight() const;

        // Optional: simple QPS limiter (0 means unlimited)
        void set_qps_limit(int qps);

        // Acquire one "API call permit" under QPS limit; blocks until allowed
        void acquire_permit();

    private:
        void worker_loop();

    private:
        mutable std::mutex mtx_;
        std::condition_variable cv_;
        std::condition_variable drained_cv_;

        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> q_;

        std::atomic<int> inflight_{0};
        bool accepting_ = true;
        bool stopping_ = false;

        // QPS limiter state
        mutable std::mutex qps_mtx_;
        int qps_ = 0; // 0 => unlimited
        std::chrono::steady_clock::time_point window_start_;
        int used_in_window_ = 0;
    };

    template <class F, class... Args>
    auto ThreadPool2::submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
        using R = std::invoke_result_t<F, Args...>;

        auto task_ptr = std::make_shared<std::packaged_task<R()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );

        std::future<R> fut = task_ptr->get_future();

        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!accepting_) {
                throw std::runtime_error("ThreadPool2: not accepting new tasks");
            }
            q_.emplace([task_ptr]() { (*task_ptr)(); });
        }
        cv_.notify_one();
        return fut;
    }

} // namespace fre
