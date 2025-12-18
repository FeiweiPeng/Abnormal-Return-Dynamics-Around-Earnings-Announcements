#include "StockUtils.h"
#include "ThreadUtils.h"
#include "CurlUtils.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <thread>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <cmath>
#include <map>
#include <string>

namespace fre {

    using namespace std;

    // Load benchmark (index/ETF) prices into a date->price map using the API price series fetch.
    map<string, double> loadBenchmarkPrices(const string& ticker,
                                        const string& fromDate,
                                        const string& toDate){
        map<string, double> benchmarkPrices;

        CURL* curl = curl_easy_init();
        if (!curl) {
            cerr << "[BenchmarkInit] Failed to initialize CURL for benchmark." << endl;
            return benchmarkPrices;
        }

        vector<PriceData> series =
            FetchPriceSeriesWithDates(curl, ticker, fromDate, toDate, fromDate);

        curl_easy_cleanup(curl);

        if (series.empty()) {
            cerr << "[BenchmarkInit] No data returned for benchmark ticker " << ticker << endl;
            return benchmarkPrices;
        }

        for (const auto& pd : series) {
            benchmarkPrices[pd.date] = pd.price;
        }

        cout << "[BenchmarkInit] Loaded " << benchmarkPrices.size()
            << " benchmark price entries for " << ticker << endl;

        return benchmarkPrices;
    }

    // Enrich existing Stock objects in stockMap with company name and sector from a CSV file.
    void enrichStocksWithSectorInfo(map<string, Stock>& stockMap,
                                const string& filename){
        ifstream fin(filename);
        if (!fin.is_open()) {
            cerr << "Error opening file: " << filename << endl;
            return;
        }

        string line;
        getline(fin, line);

        while (getline(fin, line)) {
            if (line.empty()) continue;

            stringstream ss(line);
            string ticker, companyName, sector;

            getline(ss, ticker, ',');
            getline(ss, companyName, ',');
            getline(ss, sector, ',');

            auto it = stockMap.find(ticker);
            if (it != stockMap.end()) {
                it->second.setCompanyName(companyName);
                it->second.setSector(sector);
            }
        }

        fin.close();
    }

    // Load earnings data from CSV and populate stockMap with Stock objects keyed by ticker.
    void enrichStocksWithGroupInfo(map<string, Stock>& stockMap, const string& filename)
    {
        ifstream fin(filename);
        if (!fin.is_open()) {
            cerr << "[StockUtils] Error opening file: " << filename << endl;
            return;
        }

        string line;
        getline(fin, line);

        int count = 0;
        int errorCount = 0;

        while (getline(fin, line)) {
            if (line.empty()) continue;

            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            stringstream ss(line);
            string ticker, date, period_ending;
            string estimate_str, reported_str, surprise_str, surprise_pct_str;

            getline(ss, ticker, ',');
            getline(ss, date, ',');
            getline(ss, period_ending, ',');
            getline(ss, estimate_str, ',');
            getline(ss, reported_str, ',');
            getline(ss, surprise_str, ',');
            getline(ss, surprise_pct_str); // last field

            // minimal validation: must have ticker + date at least
            if (ticker.empty() || date.empty()) continue;


            try {
                double estimate      = stod(estimate_str);
                double reported      = stod(reported_str);
                double surprise      = stod(surprise_str);
                double surprise_pct  = stod(surprise_pct_str);

                Stock s;
                s.setEarningData(ticker,
                                 date,
                                 period_ending,
                                 estimate,
                                 reported,
                                 surprise,
                                 surprise_pct);

                stockMap[ticker] = s;
                count++;

            } catch (...) {
                errorCount++;
            }
        }

        fin.close();
        cout << "[StockUtils] Successfully loaded " << count << " stocks from CSV.";
        if (errorCount > 0) {
            cout << " (Skipped " << errorCount << " invalid rows)";
        }
        cout << endl;
    }

    // Build a sorted list of trading dates from a date->price benchmark map.
    vector<string>
    createTradingDaysList(const map<string, double>& benchmarkPrices)
    {
        vector<string> tradingDays;
        tradingDays.reserve(benchmarkPrices.size());

        for (auto it = benchmarkPrices.begin(); it != benchmarkPrices.end(); ++it) {
            const string& date = it->first;
            tradingDays.push_back(date);
        }

        sort(tradingDays.begin(), tradingDays.end());

        return tradingDays;
    }

    // Given an event date and window size N, compute the valid trading window [fromDate, toDate].
    // If eventDate is not a trading day, adjust to the previous trading day and record a warning.
    // Return false if there are not enough trading days before/after the event to form the window.
    bool getTradingWindow(
    const vector<string>& tradingDays,
    const string& eventDate,
    int windowSizeN,
    string& adjustedEventDate,
    string& fromDate,
    string& toDate,
    map<string, string>& tradingDayWarnings)
    {
        auto it = find(tradingDays.begin(), tradingDays.end(), eventDate);

        if (it == tradingDays.end()) {

            auto lb = lower_bound(tradingDays.begin(), tradingDays.end(), eventDate);

            if (lb == tradingDays.begin()) {
                tradingDayWarnings[eventDate] =
                    "Error: No trading day before " + eventDate + ".";
                return false;
            }

            adjustedEventDate = *(lb - 1);
            it = lb - 1;

            tradingDayWarnings[eventDate] =
                string("Adjusted event day: ") + eventDate +
                " → " + adjustedEventDate + " (previous trading day)\n";
        }
        else {
            adjustedEventDate = eventDate;
        }

        int eventIndex = it - tradingDays.begin();
        bool ok = true;
        string warn = tradingDayWarnings[eventDate];

        if (eventIndex < windowSizeN) {
            ok = false;
            warn += "Insufficient days BEFORE event. Needed "
                + to_string(windowSizeN) +
                ", only " + to_string(eventIndex) + ".\n";
        }

        int daysAfter = tradingDays.end() - it - 1;
        if (daysAfter < windowSizeN) {
            ok = false;
            warn += "Insufficient days AFTER event. Needed "
                + to_string(windowSizeN) +
                ", only " + to_string(daysAfter) + ".\n";
        }

        if (!ok) {
            tradingDayWarnings[eventDate] = warn;
            return false;
        }

        fromDate = tradingDays[eventIndex - windowSizeN];
        toDate   = tradingDays[eventIndex + windowSizeN];

        return true;
    }

    // Process all stocks: build a trading calendar from benchmark, compute benchmark returns,
    // fetch each stock’s event-window price series concurrently, then compute returns and abnormal returns.
    void SETALLStocks(map<string, Stock>& stockMap,
                       const map<string, double>& benchmarkPrices,
                       int N,
                       vector<string>& warnings,
                       map<string, string>& tradingDayWarnings)
    {
        if (stockMap.empty()) {
            cout << "No stocks to process." << endl;
            return;
        }

        if (benchmarkPrices.empty()) {
            cout << "No benchmark prices (IWV) provided." << endl;
            return;
        }

        vector<string> tradingDays = createTradingDaysList(benchmarkPrices);
        if (tradingDays.empty()) {
            cout << "Trading days list is empty (from benchmarkPrices)." << endl;
            return;
        }

        map<string, double> benchmarkReturns;

        double prevPrice = 0.0;
        bool   first = true;
        for (const auto& kv : benchmarkPrices) {
            const string& date  = kv.first;
            double        price = kv.second;

            if (first) {
                prevPrice = price;
                first = false;
                continue;
            }

            if (price <= 0.0 || prevPrice <= 0.0) {
                prevPrice = price;
                continue;
            }

            double rmt = log(price / prevPrice);
            benchmarkReturns[date] = rmt;
            prevPrice = price;
        }

        if (benchmarkReturns.empty()) {
            cout << "Benchmark returns series is empty. Check benchmarkPrices." << endl;
            return;
        }

        struct StockJob {
            string ticker;
            string announcementDate;
        };

        vector<StockJob> jobs;
        jobs.reserve(stockMap.size());

        for (map<string, Stock>::const_iterator it = stockMap.begin();
            it != stockMap.end(); ++it)
        {
            StockJob job;
            job.ticker = it->first;
            job.announcementDate = it->second.getAnnouncementDate();
            jobs.push_back(job);
        }

        // ====== ThreadPool v2 ======
        ThreadPool2 pool(12);
        vector<future<void>> futures;
        pool.set_qps_limit(30);
        futures.reserve(jobs.size());

        mutex warnMutex;
        mutex stockMutex;

        atomic<int> finishedCount(0);
        atomic<int> okCount(0);
        const int totalJobs = static_cast<int>(jobs.size());

        thread progressThread([&]() {
            while (finishedCount < totalJobs) {
                int done = finishedCount.load();
                int ok   = okCount.load();
                int pct  = (totalJobs == 0) ? 0 : (done * 100 / totalJobs);

                cout << "\rProcessing stocks: "
                    << done << "/" << totalJobs
                    << " (" << pct << "%) "
                    << "Success: " << ok << flush;

                this_thread::sleep_for(chrono::milliseconds(100));
            }
            cout << endl;
        });

        for (size_t i = 0; i < jobs.size(); ++i) {

            StockJob job = jobs[i];

            futures.push_back(pool.submit([&, job]() {

                CURL* curl = curl_easy_init();
                if (!curl) {
                    {
                        lock_guard<mutex> lock(warnMutex);
                        warnings.push_back("Failed to initialize CURL for " + job.ticker);
                    }
                    ++finishedCount;
                    return;
                }

                string adjustedEventDate;
                string fromDate;
                string toDate;

                bool windowOK = getTradingWindow(
                    tradingDays,
                    job.announcementDate,
                    N,
                    adjustedEventDate,
                    fromDate,
                    toDate,
                    tradingDayWarnings
                );

                if (!windowOK) {
                    {
                        lock_guard<mutex> lock(warnMutex);
                        string msg = "Cannot build event window for " + job.ticker +
                                    " (event date = " + job.announcementDate + ").";

                        auto itWarn = tradingDayWarnings.find(job.announcementDate);
                        if (itWarn != tradingDayWarnings.end()) {
                            msg += " Details: " + itWarn->second;
                        }
                        warnings.push_back(msg);
                    }
                    curl_easy_cleanup(curl);
                    ++finishedCount;
                    return;
                }

                // ====== rate limit v2 ======
                pool.acquire_permit();

                vector<PriceData> priceSeries =
                    FetchPriceSeriesWithDates(curl,
                                            job.ticker,
                                            fromDate,
                                            toDate,
                                            adjustedEventDate);

                curl_easy_cleanup(curl);

                const int expectedPoints = 2 * N + 1;
                if (static_cast<int>(priceSeries.size()) != expectedPoints) {
                    lock_guard<mutex> lock(warnMutex);
                    warnings.push_back(
                        "Price series size mismatch for " + job.ticker +
                        ". Expected " + to_string(expectedPoints) +
                        " points, got " + to_string(priceSeries.size())
                    );
                    ++finishedCount;
                    return;
                }

                if (!priceSeries.empty()) {

                    lock_guard<mutex> lock(stockMutex);

                    map<string, Stock>::iterator itStock = stockMap.find(job.ticker);
                    if (itStock == stockMap.end()) {
                        {
                            lock_guard<mutex> wlock(warnMutex);
                            warnings.push_back("Ticker " + job.ticker +
                                            " disappeared from stockMap unexpectedly.");
                        }
                        ++finishedCount;
                        return;
                    }

                    Stock& stockRef = itStock->second;

                    stockRef.setPrices(priceSeries);
                    stockRef.getAdjClosePrice();

                    Vector retSeries = stockRef.CalcReturns();
                    if (static_cast<int>(retSeries.size()) != 2 * N) {
                        {
                            lock_guard<mutex> wlock(warnMutex);
                            warnings.push_back(
                                "Return series size mismatch for " + job.ticker +
                                ". Expected " + to_string(2 * N) +
                                " returns, got " + to_string(retSeries.size())
                            );
                        }
                        stockRef.setPrices(vector<PriceData>());
                        ++finishedCount;
                        return;
                    }

                    stockRef.CalcCumReturns();

                    stockRef.setStartEndDate(fromDate, toDate);

                    Vector benchWindow;
                    benchWindow.reserve(2 * N);

                    auto itEvent = find(tradingDays.begin(), tradingDays.end(), adjustedEventDate);
                    if (itEvent == tradingDays.end()) {
                        {
                            lock_guard<mutex> wlock(warnMutex);
                            warnings.push_back("Adjusted event date " + adjustedEventDate +
                                            " not found in tradingDays for " + job.ticker);
                        }
                        ++finishedCount;
                        return;
                    }

                    int eventIdx = static_cast<int>(itEvent - tradingDays.begin());

                    for (int offset = -N + 1; offset <= N; ++offset) {
                        int idx = eventIdx + offset;
                        if (idx < 0 || idx >= static_cast<int>(tradingDays.size())) {
                            {
                                lock_guard<mutex> wlock(warnMutex);
                                warnings.push_back("Benchmark index out of range for " + job.ticker +
                                                " at offset " + to_string(offset));
                            }
                            ++finishedCount;
                            return;
                        }

                        const string& date = tradingDays[idx];
                        auto itBR = benchmarkReturns.find(date);
                        if (itBR == benchmarkReturns.end()) {
                            {
                                lock_guard<mutex> wlock(warnMutex);
                                warnings.push_back("No benchmark return for date " + date +
                                                " when processing " + job.ticker);
                            }
                            ++finishedCount;
                            return;
                        }

                        benchWindow.push_back(itBR->second);
                    }

                    if (static_cast<int>(benchWindow.size()) != 2 * N) {
                        {
                            lock_guard<mutex> wlock(warnMutex);
                            warnings.push_back("Benchmark window size mismatch for " + job.ticker +
                                            ". Expected " + to_string(2 * N) +
                                            " got " + to_string(benchWindow.size()));
                        }
                        ++finishedCount;
                        return;
                    }

                    stockRef.CalcAbnormReturns(benchWindow);

                    ++okCount;
                }

                ++finishedCount;
            }));
        }

        // ====== wait v2 ======
        pool.drain();
        for (auto& f : futures) f.get();

        progressThread.join();

        cout << "\nProcessing complete. Successfully processed "
            << okCount << " out of " << totalJobs << " stocks."
            << endl;
    }

}
