#include "StockStructure.h"
#include "MatrixOperator.h"
#include <string>
#include <vector>
#include <cmath>
#include <iomanip> 

namespace fre {

    void Stock::setPrices(const vector<PriceData>& pdata) {
        PriceSeries = pdata;
    }

    void Stock::setStartEndDate(const string& s, const string& e) {
        WindowStart = s;
        WindowEnd = e;
    }

    void Stock::setEarningData(const string& ticker_,
                               const string& ann_,
                               const string& pend_,
                               double est_, double rpt_,
                               double spr_, double sprpct_) 
    {
        ticker = ticker_;
        AnnDate = ann_;
        PeriodEndDate = pend_;
        EstEps = est_;
        RptEps = rpt_;
        EpsSurprise = spr_;
        EpsSurprisePct = sprpct_;
    }

    Vector Stock::getAdjClosePrice() {
        int n = PriceSeries.size();
        Vector temp(n);

        for (int i = 0; i < n; ++i) {
            temp[i] = PriceSeries[i].price;
        }

        AdjPricesVec = temp;
        return AdjPricesVec;
    }

    bool Stock::operator<(const Stock& rhs) const {
        return EpsSurprisePct < rhs.EpsSurprisePct;
    }

    Vector Stock::CalcReturns() {
        Vector px = AdjPricesVec;
        int n = px.size();

        Vector r;
        for (int i = 1; i < n; ++i) {
            double logr = log(px[i] / px[i - 1]);
            r.push_back(logr);
        }

        LogReturnVec = r;
        return LogReturnVec;
    }

    Vector Stock::CalcCumReturns() {
        Vector r = CalcReturns();

        Vector cr(r.size());
        double accum = 0.0;

        for (size_t i = 0; i < r.size(); ++i) {
            accum += r[i];
            cr[i] = accum;
        }

        CumReturnVec = cr;
        return CumReturnVec;
    }

    Vector Stock::CalcAbnormReturns(Vector& bmret) {
        Vector r = CalcReturns();
        Vector ab(r.size());

        for (size_t i = 0; i < r.size(); ++i) {
            ab[i] = r[i] - bmret[i];
        }

        AbReturnVec = ab;
        return AbReturnVec;
    }


    ostream& operator<<(ostream& os, const Stock& s)
    {
        os << "=========== STOCK SUMMARY: " << s.getTicker() << " ===========" << endl;

        // ------- 1. Price Series ------- //
        os << "\n[1] Adjusted Prices" << endl;
        const auto& px = s.getPrices();

        if (px.empty()) {
            os << "(no price records available)" << endl;
        } else {
            os << left << setw(14) << "Date" << setw(10) << "Price" << endl;
            for (const auto& p : px) {
                os << left << setw(14) << p.date
                << setw(10) << fixed << setprecision(4) << p.price << endl;
            }
        }

        // ------- 2. Cumulative Log Returns ------- //
        os << "\n[2] Cumulative Returns" << endl;
        Vector cum = s.getCumReturns();

        if (cum.empty()) {
            os << "(no cumulative return data)" << endl;
        } else {
            for (std::size_t i = 0; i < cum.size(); ++i) {
                os << "Day " << setw(3) << i << ": "
                << fixed << setprecision(5) << cum[i] << endl;
            }
        }

        // ------- 3. Group & Surprise ------- //
        os << "\n[3] Classification" << endl;
        os << "Group:          " << s.getGroup() << endl;
        os << "Surprise (%):   "
        << fixed << setprecision(2) << s.getSurprisePercent() << "%" << endl;

        // ------- 4. Earnings Announcement ------- //
        os << "\n[4] Earnings Information" << endl;
        os << "Announcement:   " << s.getAnnouncementDate() << endl;
        os << "Period Ending:  " << s.getPeriodEnding() << endl;

        os << "Estimate:       " << fixed << setprecision(3) << s.getEstimateEarning() << endl;
        os << "Reported:       " << fixed << setprecision(3) << s.getReportedEarning() << endl;
        os << "Surprise:       " << fixed << setprecision(3) << s.getSurprise() << endl;

        os << "============================================" << endl;

        return os;
    }

}


