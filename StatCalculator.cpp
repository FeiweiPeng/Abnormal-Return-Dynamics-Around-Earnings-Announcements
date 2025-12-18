#include "StatCalculator.h"
#include <cmath>
#include <iostream>

namespace fre{
    // Constructor
    // Can be defined in the header, but must be marked as inline to avoid ODR (multiple definition) issues.
    StatCalculator::StatCalculator(int N) : N_(N) {}

    // Compute statistics for a single group.
    // Compute mean and standard deviation at each time point.
    GroupStats StatCalculator::computeForOneGroup(const GroupBootstrapResult& result)
    {
        GroupStats stats;
        int T = 2 * N_;
        
        const std::vector<Vector>& AAR_samples = result.AAR_samples; // Matrix AAR_samples = result.AAR_samples also work
        const std::vector<Vector>& CAAR_samples = result.CAAR_samples;

        // Data size check
        int K = static_cast<int>(
            std::min(AAR_samples.size(), CAAR_samples.size())
        );
        if (K == 0) {
            std::cerr << "[StatCalculator] Warning: no samples for this group.\n";
            return stats; 
        }
        if (AAR_samples.size() != CAAR_samples.size()) {
            std::cerr << "[StatCalculator] Warning: AAR and CAAR sample counts mismatch.\n";
        }

        // Initialization
        // Each Vector is resized to length T and filled with zeros.
        // The original time-series samples will later be compressed into aggregate statistics.
        stats.AAR_mean.assign(T, 0.0); // Initializes all statistic vectors to length T with zeros.
        stats.AAR_std.assign(T, 0.0);
        stats.CAAR_mean.assign(T, 0.0);
        stats.CAAR_std.assign(T, 0.0);

        // -----------------------------
        // Compute mean
        // -----------------------------
        // sum
        int validSamples = 0;
        for (int k = 0; k < K; ++k){ // K is the sample size 
            const Vector& aar = AAR_samples[k]; // Extract inner time series
            const Vector& caar = CAAR_samples[k];

            if ((static_cast<int>(aar.size()) != T) ||
                (static_cast<int>(caar.size()) != T)){
                std::cerr << "[StatCalculator] Warning: sample length mismatch, skip one\n";
                continue;
            }
        
            stats.AAR_mean = stats.AAR_mean + aar;
            stats.CAAR_mean = stats.CAAR_mean +caar;

            ++validSamples; 
        }

        // Normalize to obtain mean
        if (validSamples == 0) { // defensive programming
            std::cerr << "[StatCalculator] Error: no valid samples, all bad.\n";
            return stats;  
        }

        stats.AAR_mean = stats.AAR_mean / static_cast<double>(validSamples);
        stats.CAAR_mean = stats.CAAR_mean / static_cast<double>(validSamples);

        // -----------------------------
        // Compute standard deviation
        // -----------------------------
        if (validSamples > 1) { // defensive programming 
            for (int k = 0; k < K; ++k) {
                const Vector& aar  = AAR_samples[k];
                const Vector& caar = CAAR_samples[k];

                if (static_cast<int>(aar.size()) != T ||
                    static_cast<int>(caar.size()) != T) {
                    // This sample was also skipped during mean calculation
                    continue;
                }

                // MatrixOperator.cpp
                Vector diffA = aar  - stats.AAR_mean;
                Vector diffC = caar - stats.CAAR_mean;
                stats.AAR_std  = stats.AAR_std  + (diffA * diffA);
                stats.CAAR_std = stats.CAAR_std + (diffC * diffC);
            }

            for (int t = 0; t < T; ++t) {
                stats.AAR_std[t]  = std::sqrt(stats.AAR_std[t]  / (validSamples - 1)); // Sample standard deviation: divide by (validSamples - 1)
                stats.CAAR_std[t] = std::sqrt(stats.CAAR_std[t] / (validSamples - 1)); // Dependency on MatrixOperator.cpp
            }
        }
        else {
            // validSamples <= 1:
            // Standard deviation remains 0.0, as already initialized.
            std::cerr << "[StatCalculator] Warning: less than 2 valid samples, std set to 0.\n";
        }

        return stats;
    }

    // Compute statistics for all three groups: Miss, Meet, Beat
    void StatCalculator::computeForAllGroup(const GroupBootstrapResult& missResult,
                                            const GroupBootstrapResult& meetResult,
                                            const GroupBootstrapResult& beatResult)
    {
        missStats_ = computeForOneGroup(missResult);
        meetStats_ = computeForOneGroup(meetResult);
        beatStats_ = computeForOneGroup(beatResult);

        // Prepare data for gnuplot (using CAAR_mean only)
        // Order: [0] = Beat, [1] = Meet, [2] = Miss
        caarMeanForGnuplot_.resize(3);
        caarMeanForGnuplot_[0] = beatStats_.CAAR_mean;
        caarMeanForGnuplot_[1] = meetStats_.CAAR_mean;
        caarMeanForGnuplot_[2] = missStats_.CAAR_mean;

        buildResultMatrix();
    }

    // Reduce a full GroupStats object into a single summary row:
    // (avg AAR mean, avg AAR std, final CAAR mean, final CAAR std)
    void StatCalculator::reduceStats(const GroupStats& stats, Vector& row){
        int T = stats.AAR_mean.size();
        if (T == 0) return;

        double sumAAR = 0.0;
        double sumStdA = 0.0;

        for (int t = 0; t < T; ++t) {
            sumAAR  += stats.AAR_mean[t];
            sumStdA += stats.AAR_std[t];
        }

        row[0] = sumAAR / T;
        row[1] = sumStdA / T;
        row[2] = stats.CAAR_mean.back();
        row[3] = stats.CAAR_std.back();
    }

    // Build a 3×4 result matrix summarizing Miss / Meet / Beat groups
    void StatCalculator::buildResultMatrix(){
        resultMatrix.assign(3, Vector(4, 0.0));
        
        // Row mapping:
        // 0 = Miss, 1 = Meet, 2 = Beat
        reduceStats(missStats_, resultMatrix[0]);
        reduceStats(meetStats_, resultMatrix[1]);
        reduceStats(beatStats_, resultMatrix[2]);
    }

}
