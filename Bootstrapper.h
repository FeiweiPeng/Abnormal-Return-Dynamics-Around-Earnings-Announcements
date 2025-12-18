#pragma once 
#include <vector>
#include <random>
#include "StockStructure.h"
#include "MatrixOperator.h"


namespace fre{
    // Container for bootstrap results of a single group
    // Each row corresponds to one bootstrap sampling
    struct GroupBootstrapResult{
        Matrix AAR_samples; 
        Matrix CAAR_samples; 
        // std::vector<Vector> CAAR_samples
        // where Vector is `typedef vector<double>`
    };

    class Bootstrapper{
        private:  
            int N_;  // Half window length (event window size = 2 * N_)
            int numSamples_; // Number of bootstrap repetitions
            int sampleSize_; // Number of stocks sampled in each bootstrap draw
        public:
            // Constructor
            Bootstrapper(int N, int numSamples = 40, int sampleSize = 30);

            // Bootstrap one group
            GroupBootstrapResult bootstrapSingleGroup(const std::vector<Stock>& group);

            // Bootstrap all three group
            void runBootstrap(const std::vector<Stock>& missGroup, 
                              const std::vector<Stock>& meetGroup, 
                              const std::vector<Stock>& beatGroup, 
                              GroupBootstrapResult& missResult,
                              GroupBootstrapResult& meetResult,
                              GroupBootstrapResult& beatResult);
            
            // Accessor
            int getWindowSize() const {return N_;}
            int getNumSamples() const {return numSamples_;}
    };
}