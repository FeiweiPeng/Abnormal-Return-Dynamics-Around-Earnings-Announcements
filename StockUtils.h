#pragma once

#include <ctime>
#include <string>
#include <vector>
#include <iomanip>
#include <map>
#include <curl/curl.h>

#include "CurlUtils.h"
#include "StockStructure.h"        

namespace fre {

    map<string, double> loadBenchmarkPrices(const string& ticker,
                                            const string& fromDate,
                                            const string& toDate);

    void enrichStocksWithSectorInfo(std::map<std::string, Stock>& stockMap, const std::string& filename);

    void enrichStocksWithGroupInfo(std::map<std::string, Stock>& stockMap, const std::string& filename);

    std::vector<std::string>createTradingDaysList(const std::map<std::string, double>& benchmarkPriceMap);

    bool getTradingWindow(const std::vector<std::string>& tradingDays,
                          const std::string& eventDate,
                          int windowSizeN,
                          std::string& adjustedEventDate,
                          std::string& fromDate,
                          std::string& toDate,
                          std::map<std::string, std::string>& tradingDayWarnings);

    void SETALLStocks(map<string, Stock>& stockMap,
                      const map<string, double>& benchmarkPrices, 
                      int N,
                      vector<string>& warnings,
                      map<string, string>& tradingDayWarnings);

}
