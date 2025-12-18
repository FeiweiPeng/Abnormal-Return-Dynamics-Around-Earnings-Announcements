#pragma once 

#include <vector>
#include "MatrixOperator.h"
#include "Bootstrapper.h"

namespace fre{
    struct GroupStats{
        // Stores time-series statistics for one group (Beat / Meet / Miss)
        Vector AAR_mean;
        Vector AAR_std; 
        Vector CAAR_mean;
        Vector CAAR_std;
    };

    class StatCalculator{
        private:  
            int N_;
            GroupStats missStats_;
            GroupStats meetStats_;
            GroupStats beatStats_;
            Matrix resultMatrix; // Aggregated summary matrix for output

            // Data structure prepared specifically for gnuplot visualization
            // Stores CAAR_mean for three groups in the order:
            // [0] = Beat, [1] = Meet, [2] = Miss
            std::vector<Vector> caarMeanForGnuplot_; // Matrix caarMeanForGnuplot_ also work

            // Helper function used by buildResultMatrix()
            // Reduces a GroupStats object into a single summary row
            void reduceStats(const GroupStats& stats, Vector& row);

        public:  
            // Constructor
            explicit StatCalculator(int N); // 'explicit' prevents unintended implicit conversion from int

            // Compute mean and standard deviation of AAR / CAAR
            // for a single group based on bootstrap results
            GroupStats computeForOneGroup(const GroupBootstrapResult& result);
            
            // Compute statistics for all three groups
            void computeForAllGroup(const GroupBootstrapResult& missResult, 
                         const GroupBootstrapResult& meetResult, 
                         const GroupBootstrapResult& beatResult);
            
            void buildResultMatrix();
            
            // accessor
            int getN() const {return N_;}
            const GroupStats& getMissStats() const {return missStats_;}
            const GroupStats& getMeetStats() const {return meetStats_;}
            const GroupStats& getBeatStats() const {return beatStats_;}
            const Matrix& getResultMatrix() const { return resultMatrix;}
            
            // Accessor for gnuplot-ready CAAR mean time series
            const std::vector<Vector>& getCAARMeanForGnuplot() const { return caarMeanForGnuplot_; }
    };
}
