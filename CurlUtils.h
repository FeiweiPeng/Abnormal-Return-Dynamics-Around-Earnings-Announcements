#pragma once

#include <string>
#include <vector>
#include <curl/curl.h>

#include "StockStructure.h" 
using namespace std;
namespace fre 
{

    struct MemoryStruct 
    {
        char*  memory;
        size_t size;
    };

    string read_api_token(const string& filenam);

    size_t write_data2(void* ptr, size_t size, size_t nmemb, void* data);

    vector<PriceData> FetchPriceSeriesWithDates(
        CURL* curlHandle,
        const string& ticker,
        const string& fromDate,
        const string& toDate,
        const string& eventDate
    );

}
