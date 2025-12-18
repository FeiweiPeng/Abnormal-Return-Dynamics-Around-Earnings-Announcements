#include "CurlUtils.h"

#include <sstream>
#include <fstream>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>

using namespace std;

namespace fre {

    // Read API token from a local file (first line).
    // Throws an exception if the file cannot be opened.
    string read_api_token(const string& filename){
        ifstream fin(filename);
        string token;
        if (fin.is_open()) {
            getline(fin, token);
            fin.close();
        } else {
            throw runtime_error("Failed to open API token file: " + filename);
        }
        return token;
    }

    // Return a cached API token string (loaded once from api_token.txt).
    // The static local ensures the token is read only once for the entire program.
    static const string& get_api_token() {
        static const string token = read_api_token("api_token.txt");
        return token;
    }

    // libcurl write callback: append received bytes into a growing MemoryStruct buffer.
    // Returns the number of bytes actually stored; returning 0 signals failure to libcurl.
    size_t write_data2(void* ptr, size_t size, size_t nmemb, void* data) {
        size_t realSize = size * nmemb;
        MemoryStruct* mem = (MemoryStruct*)data;

        char* newMemory = (char*)(
            mem->memory
                ? realloc(mem->memory, mem->size + realSize + 1)
                : malloc(mem->size + realSize + 1)
        );

        if (!newMemory) {
            cerr << "[Error] Memory allocation failed in write_data2()" << endl;
            return 0;
        }

        mem->memory = newMemory;
        memcpy(mem->memory + mem->size, ptr, realSize);
        mem->size += realSize;
        mem->memory[mem->size] = '\0';

        return realSize;
    }

    // Normalize date strings into "YYYY-MM-DD".
    // If the input already contains '-', it is assumed to be normalized and returned as-is.
    // Otherwise it converts formats like "YYYY/M/D" into zero-padded "YYYY-MM-DD".
    static string normalize_date(const string& rawDate) {
        if (rawDate.find('-') != string::npos) return rawDate;

        istringstream ss(rawDate);
        string year, month, day;
        getline(ss, year, '/');
        getline(ss, month, '/');
        getline(ss, day);

        if (month.size() == 1) month = "0" + month;
        if (day.size()   == 1) day   = "0" + day;

        return year + "-" + month + "-" + day;
    }

    // Fetch daily EOD price data (CSV) for a given ticker and date range from EODHistoricalData.
    // Parses the CSV, keeps only adjusted close as pd.price, and labels each date relative to eventDate.
    // Retries up to kMaxAttempts with simple backoff if the request fails or returns empty data.
    vector<PriceData> FetchPriceSeriesWithDates(
        CURL* curlHandle,
        const string& ticker,
        const string& fromDate,
        const string& toDate,
        const string& eventDate
    ) {
        vector<PriceData> result;

        if (!curlHandle) {
            cerr << "[CurlUtils] Invalid CURL handle." << endl;
            return result;
        }

        const string& token = get_api_token();
        if (token.empty()) {
            cerr << "[CurlUtils] Empty API token, skip ticker "
                << ticker << endl;
            return result;
        }

        string endpoint = "https://eodhistoricaldata.com/api/eod/";
        string url = endpoint + ticker + ".US"
                + "?from="      + fromDate
                + "&to="        + toDate
                + "&api_token=" + token
                + "&period=d";

        const int kMaxAttempts = 5;
        int attempt = 0;

        while (attempt < kMaxAttempts) {

            MemoryStruct buffer;
            buffer.memory = NULL;
            buffer.size   = 0;

            curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, write_data2);
            curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &buffer);

            CURLcode rc = curl_easy_perform(curlHandle);
            long http_code = 0;
            curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &http_code);
            
            if (rc == CURLE_OK && http_code == 429) {
                // exponential backoff + jitter: 1s, 2s, 4s, 8s, 16s 
                int backoff_ms = (1 << attempt) * 1000;
                int jitter_ms  = rand() % 200; 
                this_thread::sleep_for(chrono::milliseconds(backoff_ms + jitter_ms));

                if (buffer.memory) free(buffer.memory);
                ++attempt;
                continue;
            }

            if (rc == CURLE_OK && http_code == 200 && buffer.memory != NULL && buffer.size > 0) {

                string csvText(buffer.memory, buffer.size);
                stringstream csvStream(csvText);
                string line;

                if (!getline(csvStream, line)) {
                    if (buffer.memory) free(buffer.memory);
                    break;
                }

                vector<PriceData> series;
                series.reserve(256);

                while (getline(csvStream, line)) {
                    if (line.empty()) continue;

                    vector<string> fields;
                    string token;
                    stringstream ls(line);

                    while (getline(ls, token, ',')) {
                        fields.push_back(token);
                    }

                    if (fields.size() < 6) continue;

                    string date   = normalize_date(fields[0]);
                    string adjStr = fields[5];

                    try {
                        double adjClose = stod(adjStr);
                        PriceData pd;
                        pd.date  = date;
                        pd.price = adjClose;
                        series.push_back(pd);
                    } catch (...) {
                        continue;
                    }
                }

                int eventIndex = -1;
                for (size_t i = 0; i < series.size(); ++i) {
                    if (series[i].date == eventDate) {
                        eventIndex = (int)i;
                        break;
                    }
                }

                if (eventIndex != -1) {
                    for (size_t i = 0; i < series.size(); ++i) {
                        int offset = (int)i - eventIndex;
                        series[i].date_label = "date_" + to_string(offset);
                    }
                }

                result.swap(series);

                if (buffer.memory) free(buffer.memory);

                break;
            }
            //TEST CODE
            // if (rc != CURLE_OK || http_code != 200) {
            //     cerr << "[CurlUtils] HTTP " << http_code
            //         << " for " << ticker
            //         << " (attempt " << attempt + 1 << ")" << endl;
            // }

            if (buffer.memory) free(buffer.memory);

            ++attempt;
            if (attempt < kMaxAttempts) {
                this_thread::sleep_for(chrono::seconds(attempt));
            }
        }
        return result;
    }

} // namespace fre
