#pragma once
#include "MatrixOperator.h"
#include <string>
#include <vector>

using namespace std;

namespace fre {

    struct PriceData {
        string date;
        string date_label;
        double price;
    };

    class Stock {
    private:
        string ticker;
        string AnnDate;
        string PeriodEndDate;

        double EstEps;
        double RptEps;
        double EpsSurprise;
        double EpsSurprisePct;

        string GroupTag;
        string WindowStart;
        string WindowEnd;

        vector<PriceData> PriceSeries;

        Vector AdjPricesVec;
        Vector LogReturnVec;
        Vector CumReturnVec;
        Vector AbReturnVec;

        string FullCompanyName;
        string IndustryName;

    public:
        Stock()
            : ticker(""), AnnDate(""), PeriodEndDate(""),
              EstEps(0.0), RptEps(0.0),
              EpsSurprise(0.0), EpsSurprisePct(0.0),
              GroupTag(""), WindowStart(""), WindowEnd(""),
              PriceSeries(), AdjPricesVec(),
              LogReturnVec(), CumReturnVec(), AbReturnVec(),
              FullCompanyName(""), IndustryName("") {}

        Stock(string tck, string adate, string pend,
              double est, double rpt, double spr, double sprpct)
            : ticker(tck), AnnDate(adate), PeriodEndDate(pend),
              EstEps(est), RptEps(rpt),
              EpsSurprise(spr), EpsSurprisePct(sprpct),
              GroupTag(""), WindowStart(""), WindowEnd(""),
              PriceSeries(), AdjPricesVec(),
              LogReturnVec(), CumReturnVec(), AbReturnVec(),
              FullCompanyName(""), IndustryName("") {}

        // --- Accessors ---
        string getTicker() const { return ticker; }
        string getAnnouncementDate() const { return AnnDate; }
        string getStartDate() const { return WindowStart; }
        string getEndDate() const { return WindowEnd; }
        string getPeriodEnding() const { return PeriodEndDate; }

        double getEstimateEarning() const { return EstEps; }
        double getReportedEarning() const { return RptEps; }
        double getSurprise() const { return EpsSurprise; }
        double getSurprisePercent() const { return EpsSurprisePct; }

        const vector<PriceData>& getPrices() const { return PriceSeries; }

        Vector getReturns() const { return LogReturnVec; }
        Vector getCumReturns() const { return CumReturnVec; }
        Vector getAbnormReturns() const { return AbReturnVec; }
        string getGroup() const { return GroupTag; }

        void setCompanyName(const string& n) { FullCompanyName = n; }
        void setSector(const string& s) { IndustryName = s; }

        const string& getCompanyName() const { return FullCompanyName; }
        const string& getSector() const { return IndustryName; }

        Vector getAdjClosePrice();

        // --- Mutators ---
        void setPrices(const vector<PriceData>& pdata);
        void setStartEndDate(const string& s, const string& e);

        void setEarningData(const string& ticker_, const string& ann_, const string& pend_,
                            double est_, double rpt_, double spr_, double sprpct_);

        void setGroup(const string& g) { GroupTag = g; }

        bool operator<(const Stock& rhs) const;

        Vector CalcReturns();
        Vector CalcCumReturns();
        Vector CalcAbnormReturns(Vector& benchmarkReturns);

        friend ostream& operator<<(ostream& os, const Stock& s);
    };
}

