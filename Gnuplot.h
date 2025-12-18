#pragma once

#include <vector>
#include "MatrixOperator.h"   // for Vector typedef

namespace fre {

    //  Gnuplot wrapper class, responsible only for plotting the three CAAR curves
    class Gnuplot {
    public:
        // Accepts the CAAR data prepared by StatCalculator
        // Convention: lines[0] = Beat, lines[1] = Meet, lines[2] = Miss
        static void plotCAAR(const std::vector<Vector>& caarLines, int N);
    };

} 
