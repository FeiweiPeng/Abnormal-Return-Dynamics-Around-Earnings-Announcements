#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include <curl/curl.h>

#include "StockStructure.h"
#include "StockUtils.h"
#include "StockGrouper.h"
#include "CurlUtils.h"
#include "Bootstrapper.h"
#include "Gnuplot.h"
#include "MatrixOperator.h"
#include "StatCalculator.h"
#include "ThreadUtils.h"

using namespace std;
using namespace fre;

map<string, Stock> g_stockMap;  // [From StockStructure.h]
Stock g_iwvBenchmark;  // [From StockStructure.h]
bool g_dataLoaded = false;
bool g_calcReady = false;
int default_N = 60; 
StatCalculator* g_statCalc = nullptr; // [From StatCalculator.h]
const int W_T   = 6;
const int W_COL = 12;


int main() 
{
    curl_global_init(CURL_GLOBAL_ALL);

    // ---------------------------------------------------------
    // Phase 1: Static data download and initialization
    // ---------------------------------------------------------

    // ---------------------------------------------------------
    // Step 1: Read the CSV into a Map
    // ---------------------------------------------------------
    string earningFile = "Russell3000EarningsAnnouncements.csv";
    cout << "[Step 1] Loading earnings data directly into Map..." << endl;
    enrichStocksWithGroupInfo(g_stockMap, earningFile); // [From StockUtils.h] read CSV and populate Map
    if (g_stockMap.empty()) {cerr << "[Error] Failed to load stocks. Please check the CSV file." << endl; return 1;}
    cout << "   -> Loaded " << g_stockMap.size() << " records." << endl;

    // ---------------------------------------------------------
    // Step 2: Add Sector and Company Name information
    // ---------------------------------------------------------
    cout << "[Step 2] Enriching stocks with Sector/Name info..." << endl;
    string sectorFile = "iShares-Russell-3000-ETF_fund.csv"; 
    enrichStocksWithSectorInfo(g_stockMap, sectorFile); // [From StockUtils.h] add sector info
    cout << "   -> Enrichment complete." << endl;


    // ---------------------------------------------------------
    // Step 3: Prepare for Grouping (Split by Sector)
    // ---------------------------------------------------------
    cout << "[Step 3] Organizing stocks by Sector..." << endl;
    auto sectorMap = StockGrouper::splitStocksBySector(g_stockMap); // [From StockGrouper.h]
    cout << "   -> Organized stocks into sector map (raw size: " << sectorMap.size() << " keys)." << endl;


    // ---------------------------------------------------------
    // Step 4: Execute the Core Grouping Logic (Beat/Meet/Miss)
    // ---------------------------------------------------------
    cout << "[Step 4] Running Sector-Neutral Grouping Algorithm..." << endl;
    StockGrouper grouper;  // [From StockGrouper.h]
    grouper.processAllSectors(sectorMap);  // [From StockGrouper.h] execute grouping logic
    grouper.printGroupSummary();  // [From StockGrouper.h] Write the group labels back to the global variable g_stockMap


    // ---------------------------------------------------------
    // Step 5: Update the Global Map
    // ---------------------------------------------------------
    cout << "[Step 5] Syncing groups to Global Map..." << endl;
    grouper.updateMapWithGroups(g_stockMap);  // [From StockGrouper.h]
    cout << "[Success] Phase 1 Complete. Ready for Menu." << endl;
    cout << "   -> Final Global Map Size: " << g_stockMap.size() << endl;
    cout << "===============================================" << endl;



    // ---------------------------------------------------------
    // Phase 2: Interactive Menu
    // ---------------------------------------------------------
    int choice;
    while(true) 
    {
        cout << "\n---------------- MENU ----------------" << endl;
        cout << "1. Enter N and Pull Data" << endl;
        cout << "2. Show Stock Info" << endl;
        cout << "3. Show Group Stats" << endl;
        cout << "4. Plot Results" << endl;
        cout << "5. Exit" << endl;
        cout << "Enter Choice: ";
        cin >> choice;

        if (cin.fail()) { 
            cin.clear(); 
            cin.ignore(1000, '\n'); 
            cout << "Invalid input. Please enter a number.\n";
            continue; }


        // =================================================
        // Option 1: Enter the time range and process the data
        // =================================================
        if (choice == 1) 
        {
            // --- A. User Input ---
            cout << "Enter N (30 <= N <= 60): "; 
            int g_N;
            cin >> g_N;
            if (g_N < 30) { g_N = default_N; cout << "[Warn] N too small, set to 60." << endl; }
            if (g_N > 60) { g_N = default_N; cout << "[Warn] N too large, set to 60." << endl; }
    
            CURL* curl = curl_easy_init();
            if (!curl) { cerr << "CURL Init failed" << endl; continue; }

            // --- B. Access Benchmark (IWV) ---
            cout << "Fetching Benchmark (IWV)..." << endl;
            vector<PriceData> iwvPrices = FetchPriceSeriesWithDates(curl, "IWV", "2023-12-01", "2025-12-30", ""); 
            if (iwvPrices.empty()) {cerr << "[Error] Failed to download IWV." << endl; curl_easy_cleanup(curl); continue;}
            
            // Process IWV
            g_iwvBenchmark.setPrices(iwvPrices); 
            g_iwvBenchmark.getAdjClosePrice();  
            g_iwvBenchmark.CalcReturns();  

            // Form Trading Calendar
            map<string, double> iwvMap;
            for (auto& p : iwvPrices) iwvMap[p.date] = p.price;
            vector<string> tradingDays = createTradingDaysList(iwvMap);
            cout << "    -> Trading Calendar built (" << tradingDays.size() << " days)." << endl;
            
            // --- C. Download all stock data in parallel ---
            cout << "Fetching prices for " << g_stockMap.size() << " stocks..." << endl;
            vector<string> warns;
            map<string, string> dateWarns;
            
            // Multithreaded download, filling in the "prices" and "returns"
            SETALLStocks(g_stockMap, iwvMap, g_N, warns, dateWarns); 
            cout << "\n===== Trading Day Warnings =====\n";
            for (const auto& p : dateWarns) {
                if (p.second.empty()) continue;
                cout << "EventDate: " << p.first << "\n"
                    << "Warning:   " << p.second << "\n"
                    << "----------------------------------------\n";
            }
            cout << "\n===== Stock-Level Warnings =====\n";
            if (warns.empty()) {
                cout << "No warnings.\n";
            } else {
                for (size_t i = 0; i < warns.size(); ++i) {
                    cout << "[" << i + 1 << "] " << warns[i] << "\n"
                    << "----------------------------------------\n";
                }
            }

            // --- D. Prepare Bootstrap Data ---
            cout << ">>> Preparing Data for Bootstrap..." << endl;
            
            vector<Stock> beatVec, meetVec, missVec;
            int validCount = StockGrouper::extractValidGroups(g_stockMap, beatVec, meetVec, missVec);

            cout << "    [Data Summary] Beat: " << beatVec.size() 
                 << ", Meet: " << meetVec.size() 
                 << ", Miss: " << missVec.size() 
                 << " (Total Valid: " << validCount << ")" << endl;

            // --- E. Run Bootstrap and Statistical Calculations ---
            
            // Each time Option 1 is re-run, first clear the old StatCalculator.
            if(g_statCalc) { delete g_statCalc; g_statCalc = nullptr; }
            
            // 40 iterations, with 30 samples taken each time
            Bootstrapper bootstrap(g_N, 40, 30);
            GroupBootstrapResult beatResult, meetResult, missResult;

            bootstrap.runBootstrap(missVec, meetVec, beatVec, missResult, meetResult, beatResult);
            
            // Create a new statistical calculator and save to global pointer.
            g_statCalc = new StatCalculator(g_N);
            g_statCalc->computeForAllGroup(missResult, meetResult, beatResult);

            cout << ">>> Calculations Complete. Data ready for plotting." << endl;

            g_dataLoaded = true;
            g_calcReady = true;
            curl_easy_cleanup(curl);
        }
        
        // =================================================
        // Option 2: Stock Query
        // =================================================
        else if (choice == 2)
        {
            if (!g_dataLoaded) {
                cout << "Data not loaded yet. Please run Option 1 first.\n";
                continue;
            }

            while (true)
            {
                string t;
                cout << "\nEnter Ticker (letters only, e.g., AAPL; Q to return): ";
                cin >> t;

                transform(t.begin(), t.end(), t.begin(), ::toupper);

                if (t == "Q") {
                    cout << "Returning to main menu...\n";
                    break;
                }

                // ---------- inline format check ----------
                bool valid = true;
                for (char c : t) {
                    if (!(isalpha(c) || c == '.' || c == '-')) {
                        valid = false;
                        break;
                    }
                }

                if (!valid) {
                    cout << "Invalid ticker format. Please use letters only.\n";
                    continue;
                }
                // ------------------------------------

                if (!g_stockMap.count(t)) {
                    cout << "Ticker not found. Please try again.\n";
                    continue;
                }

                Stock& s = g_stockMap[t];
                cout << "\n========== Information for " << t << " ==========\n";
                cout << s << endl;
                break;
            }
        }


        // =================================================
        // Option 3: Show Stats
        // =================================================
        else if (choice == 3) 
        {
            if(!g_calcReady || !g_statCalc) { cout << "Data not loaded yet. Please run Option 1 first." << endl; continue; } //Check if option1 is run
            
            bool exit = false;
            while(!exit){
                cout << "\nSelect Group:\n";
                cout << "1. Miss\n";
                cout << "2. Meet\n";
                cout << "3. Beat\n";
                cout << "4. Return to Main Menu\n";
                cout << "Enter choice 1-4: ";

                int g;
                cin >> g;
                if (cin.fail()) {
                    cin.clear();               // Clear the "fail" status
                    cin.ignore(1000, '\n');    // Clear the junk input in this row
                    cout << "Invalid input. Please enter a number 1-4.\n";
                    continue;
                }

                if (g < 1 || g > 4) {
                    cout << "Invalid selection. Please enter a number 1-4.\n";
                    continue;
                }

                if(g==4){
                    exit = true;
                    cout << "Returning to main menu...\n";
                    continue;
                }

                int idx = g - 1;

                // group name
                string groupName;
                if (g == 1) groupName = "Miss";
                else if (g == 2) groupName = "Meet";
                else groupName = "Beat";
                const Matrix& resultMatrix = g_statCalc->getResultMatrix();

                cout << "\n========== Group Summary: " << groupName << " ==========\n";
                cout << setprecision(6);
                cout << "Expected AAR   : " << resultMatrix[idx][0] << endl;
                cout << "AAR STD        : " << resultMatrix[idx][1] << endl;
                cout << "Expected CAAR  : " << resultMatrix[idx][2] << endl;
                cout << "CAAR STD       : " << resultMatrix[idx][3] << endl;

                cout << "====================================================\n";

                cout << "\nShow full time series? (y/n): ";
                char ans;
                cin >> ans;

                if (ans == 'y' || ans == 'Y')
                {
                    const GroupStats* statsptr;

                    if (g == 1) statsptr = &g_statCalc->getMissStats();
                    else if (g == 2) statsptr = &g_statCalc->getMeetStats();
                    else statsptr = &g_statCalc->getBeatStats();

                    const GroupStats& stats = *statsptr;

                    cout << "\n===== Time Series for " << groupName << " =====\n";
                    cout << left
                        << setw(W_T)   << "t"
                        << setw(W_COL) << "AAR_mean"
                        << setw(W_COL) << "AAR_std"
                        << setw(W_COL) << "CAAR_mean"
                        << setw(W_COL) << "CAAR_std"
                        << "\n";
                    int N = g_statCalc->getN();
                        // Print rows
                    for (int t = -N+1; t <= N; ++t) {
                        int date = t + N;    // map -N+1,..N → 1,..2N // cause we do not have return in the first day

                        cout << left
                            << setw(W_T)   << t
                            << setw(W_COL) << fixed << setprecision(6) << stats.AAR_mean[date-1]
                            << setw(W_COL) << fixed << setprecision(6) << stats.AAR_std[date-1]
                            << setw(W_COL) << fixed << setprecision(6) << stats.CAAR_mean[date-1]
                            << setw(W_COL) << fixed << setprecision(6) << stats.CAAR_std[date-1]
                            << "\n";
                    }
                }
            }
        }

        // =================================================
        // Option 4: Draw Graphs
        // =================================================
        else if (choice == 4) 
        {
            // 1. Check if the data is ready
            if(!g_calcReady || !g_statCalc) 
            { 
                cout << "Data not loaded yet. Please run Option 1 first." << endl; 
                continue; 
            } 
            
            Gnuplot plotter;
            plotter.plotCAAR(g_statCalc->getCAARMeanForGnuplot(), g_statCalc->getN());
        }

        // =================================================
        // Option 5: Exit
        // =================================================
        else if (choice == 5) 
        {
            cout << "Exiting program..." << endl;
            break;
        }
        
        // Handle invalid input
        else 
        {
            cout << "Invalid choice. Please enter 1-5." << endl;
        }

    }

    // =================================================
    // Program termination
    // =================================================
    
    // 1. Release the heap memory that was newly allocated in Option 1
    if(g_statCalc) 
    {
        delete g_statCalc;
        g_statCalc = nullptr;
    }

    // 2. Release global resources of Libcurl
    curl_global_cleanup();
    
    return 0;
}
