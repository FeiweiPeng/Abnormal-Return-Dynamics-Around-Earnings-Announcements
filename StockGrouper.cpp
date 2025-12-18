#include "StockGrouper.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <unordered_map>
#include <cmath>

using namespace std;
using namespace fre;


unordered_map<string, vector<Stock>> StockGrouper::splitStocksBySector(const map<string, Stock>& stockMap) 
{
    unordered_map<string, vector<Stock>> sectorStockMap;
    for (const auto& pair : stockMap) 
    {
        const Stock& stock = pair.second;
        string sector = stock.getSector();
        if (sector == "Other" || sector.empty()) {continue;}
        sectorStockMap[sector].push_back(stock);
    }
    return sectorStockMap;
}

void StockGrouper::processSingleSector(vector<Stock>& sectorStocks) 
{
    if (sectorStocks.empty()) return;
    sort(sectorStocks.begin(), sectorStocks.end()); 
    
    size_t totalCount = sectorStocks.size();
    size_t removeCountPerSide = ceil(totalCount * 0.02); 
    size_t remainingCount = totalCount - 2 * removeCountPerSide;
    
    vector<Stock> validStocks;
    if (removeCountPerSide > 0) 
    {
        auto startIt = sectorStocks.begin() + removeCountPerSide;
        auto endIt = sectorStocks.end() - removeCountPerSide;
        validStocks.assign(startIt, endIt);
    }
    else {validStocks = sectorStocks;}

    if (validStocks.empty()) return;

    size_t groupSize = remainingCount / 3;
    
    auto beginIt = validStocks.begin();
    vector<Stock> sectorMiss(beginIt, beginIt + groupSize);
    vector<Stock> sectorMeet(beginIt + groupSize, beginIt + 2 * groupSize);
    vector<Stock> sectorBeat(beginIt + 2 * groupSize, validStocks.end());

    for (auto& stock : sectorMiss) {stock.setGroup("Miss");}
    for (auto& stock : sectorMeet) {stock.setGroup("Meet");}
    for (auto& stock : sectorBeat) {stock.setGroup("Beat");}

    missGroup.insert(missGroup.end(), sectorMiss.begin(), sectorMiss.end());
    meetGroup.insert(meetGroup.end(), sectorMeet.begin(), sectorMeet.end());
    beatGroup.insert(beatGroup.end(), sectorBeat.begin(), sectorBeat.end());
}


void StockGrouper::updateMapWithGroups(map<string, Stock>& stockMap) const
{
    for (const auto& s : beatGroup) 
    {
        auto it = stockMap.find(s.getTicker());
        if (it != stockMap.end()) {it->second.setGroup("Beat");}
    }

    for (const auto& s : meetGroup) 
    {
        auto it = stockMap.find(s.getTicker());
        if (it != stockMap.end()) {it->second.setGroup("Meet");}
    }

    for (const auto& s : missGroup) 
    {
        auto it = stockMap.find(s.getTicker());
        if (it != stockMap.end()) {it->second.setGroup("Miss");}
    }

    int removedCount = 0;
    for (auto it = stockMap.begin(); it != stockMap.end(); ) 
    {
        if (it->second.getGroup().empty()) 
        {
            it = stockMap.erase(it); 
            removedCount++;
        } else {
            ++it;
        }
    }
    cout << "[StockGrouper] Map updated. Removed " << removedCount << " outliers." << endl;
}


void StockGrouper::processAllSectors(unordered_map<string, vector<Stock>>& sectorMap) 
{
    for (auto& pair : sectorMap) 
    {
        if (pair.first == "Other" || pair.first.empty()) {continue;}
        processSingleSector(pair.second);
    }
}


int StockGrouper::extractValidGroups(const map<string, Stock>& sourceMap, vector<Stock>& outBeat, vector<Stock>& outMeet, vector<Stock>& outMiss)
{
    outBeat.clear();
    outMeet.clear();
    outMiss.clear();

    int validCount = 0;

    for (const auto& pair : sourceMap) 
    {
        const Stock& s = pair.second;
        if (s.getPrices().empty()) {continue;}
        string g = s.getGroup(); 
        if (g == "Beat")      outBeat.push_back(s);
        else if (g == "Meet") outMeet.push_back(s);
        else if (g == "Miss") outMiss.push_back(s);
        validCount++;
    }

    return validCount;
}


void StockGrouper::printGroupSummary() const 
{
    cout << "=== Final Stock Group Summary ===" << endl;
    cout << "Miss group size : " << missGroup.size() << endl;
    cout << "Meet group size : " << meetGroup.size() << endl;
    cout << "Beat group size : " << beatGroup.size() << endl;
    cout << "Total grouped stocks: " << missGroup.size() + meetGroup.size() + beatGroup.size() << endl;
    cout << "==================================\n";
}


void StockGrouper::printSectorStockCount(const unordered_map<string, vector<Stock>>& sectorMap) const 
{
    cout << "\n=== Sector Stock Count ===" << endl;
    for (const auto& pair : sectorMap) 
    {
        cout << "Sector: " << pair.first << " | Stock Count: " << pair.second.size() << endl;
    }
    cout << "Total Sectors: " << sectorMap.size() << endl;
    cout << "=========================\n" << endl;
}