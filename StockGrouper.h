#pragma once
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include "StockStructure.h"

using namespace std;
using namespace fre;

class StockGrouper 
{
    private:
    vector<Stock> missGroup;
    vector<Stock> meetGroup;
    vector<Stock> beatGroup;
    
    public:
    StockGrouper() {}

    static unordered_map<string, vector<Stock>> splitStocksBySector(const map<string, Stock>& stockMap);
    void processSingleSector(vector<Stock>& sectorStocks);
    void updateMapWithGroups(map<string, Stock>& stockMap) const;
    void processAllSectors(unordered_map<string, vector<Stock>>& sectorMap);
    static int extractValidGroups(const map<string, Stock>& sourceMap, vector<Stock>& outBeat, vector<Stock>& outMeet, vector<Stock>& outMiss);
    
    void printGroupSummary() const;
    void printSectorStockCount(const unordered_map<string, vector<Stock>>& sectorMap) const;
    
    const vector<Stock>& getMissGroup() const { return missGroup; }
    const vector<Stock>& getMeetGroup() const { return meetGroup; }
    const vector<Stock>& getBeatGroup() const { return beatGroup; }
};