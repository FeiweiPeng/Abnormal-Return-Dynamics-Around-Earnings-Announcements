#include "Gnuplot.h"

#include <cstdio>    // FILE, popen, pclose
#include <iostream>  // std::cerr

namespace fre {

void Gnuplot::plotCAAR(const std::vector<Vector>& caarLines, int N)
{
    // // Basic check: need exactly 3 CAAR series
    if (caarLines.size() < 3) {
        std::cerr << "[Gnuplot] Error: need 3 groups (Beat/Meet/Miss), got "
                  << caarLines.size() << ".\n";
        return;
    }

    const Vector& beat = caarLines[0];
    const Vector& meet = caarLines[1];
    const Vector& miss = caarLines[2];

    // Check consistency of vector lengths
    int T = static_cast<int>(beat.size());
    if (T == 0) {
        std::cerr << "[Gnuplot] Error: empty CAAR series for Beat group.\n";
        return;
    }
    if (static_cast<int>(meet.size()) != T ||
        static_cast<int>(miss.size()) != T)
    {
        std::cerr << "[Gnuplot] Error: CAAR series length mismatch among groups.\n";
        return;
    }

    // Open gnuplot pipeline
    FILE* gp = popen("gnuplot -persist", "w");
    if (!gp) {
        std::cerr << "[Gnuplot] Error: failed to open gnuplot.\n";
        return;
    }

    
    // Basic plot settings: title, labels, grid, legend position
    std::fprintf(gp, "set title 'Expected CAAR for Beat / Meet / Miss'\n");
    std::fprintf(gp, "set xlabel 'Event Day (t)'\n");
    std::fprintf(gp, "set ylabel 'CAAR'\n");
    std::fprintf(gp, "set grid\n");
    std::fprintf(gp, "set key left top\n");

    // Plot all three CAAR curves on the same figure using data blocks
    std::fprintf(gp,
                 "plot '-' with lines title 'Beat', "
                 "'-' with lines title 'Meet', "
                 "'-' with lines title 'Miss'\n");

    // Event day convention:
    // index i = 0,...,T-1 corresponds to event day t = i - N
    // ---- Beat ----
    for (int i = 0; i < T; ++i) {
        int day = i - N + 1; // ⭐️ [-N+1, N]
        std::fprintf(gp, "%d %f\n", day, beat[i]);
    }
    std::fprintf(gp, "e\n");

    // ---- Meet ----
    for (int i = 0; i < T; ++i) {
        int day = i - N + 1;
        std::fprintf(gp, "%d %f\n", day, meet[i]);
    }
    std::fprintf(gp, "e\n");

    // ---- Miss ----
    for (int i = 0; i < T; ++i) {
        int day = i - N + 1;
        std::fprintf(gp, "%d %f\n", day, miss[i]);
    }
    std::fprintf(gp, "e\n");

    std::fflush(gp);
    pclose(gp);
}

} // namespace fre
