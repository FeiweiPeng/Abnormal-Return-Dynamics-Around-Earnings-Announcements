#include "Bootstrapper.h"
#include <algorithm>
#include <ctime>
#include <iostream>

namespace fre{
    Bootstrapper::Bootstrapper(int N, int numSamples, int sampleSize):  
        N_(N), numSamples_(numSamples), sampleSize_(sampleSize){}

    // Perform bootstrap sampling for a single group
    GroupBootstrapResult Bootstrapper::bootstrapSingleGroup(
        const std::vector<Stock>& group){
        GroupBootstrapResult result;

        int T = 2*N_; // Event window length

        if (group.empty()){
            std::cerr<<"[Bootstrapper] Warning size is 0, skip bootstrap.\n";
            return result;
        }

        // std::mt19937 random_engine(static_cast<unsigned>(std::time(nullptr))); // ⚠️ delete 
            // The resolution of time(nullptr) is one second.
            // Since C++ executes very fast, the three groups may 
            // end up using the same random seed,
            // which can negatively affect the bootstrap results.
            // Therefore, we use the following approach:
        std::random_device rd; 
            // std::random_device provides a non-deterministic source 
            // of randomness (when supported by the system).
        std::mt19937 random_engine(rd()); 
            // initializes a Mersenne Twister random engine (mt19937)
            //  using the value produced by random_device as the seed.
        std::uniform_int_distribution<int> dist(0, static_cast<int>(group.size()) - 1); // a more accurate version to generate randomness
        int M = std::min<int>(sampleSize_, static_cast<int>(group.size()));  // Unnecessary actually, but safer
        // Outer loop: number of bootstrap repetitions
        for (int s = 0; s < numSamples_; ++s){

            Vector aar(T, 0.0); // Length T, all elements are zero.
            Vector caar(T, 0.0);

            int usedStocks = 0; // Number of valid stocks used in this sample

            // Inner loop: draw M stocks for one bootstrap sample
            for (int i = 0; i < M; ++i){ 
                int random_index = dist(random_engine); // ‼️ sampling with replacement; the definition of Bootstrap is with replacement 
                    // ⭐️ Ensure the uniformly random for bootstrap
                    // random_index is sampled from a discrete uniform distribution, 
                    // which assigns equal probability to each integer in {0, 1, ..., group.size()-1}
                const Stock& stock = group[random_index];
                Vector ar = stock.getAbnormReturns(); // dependency on stockstructure.cpp 

                if (static_cast<int>(ar.size()) != T){
                    continue;
                }
                aar = aar + ar; // Vector addition: accumulate abnormal returns
                ++usedStocks;
            }

            if (usedStocks == 0){
                std::cerr << "[Bootstrapper] Warning: no valid stocks in this sampling. \n";
                continue;
            }

            aar = aar / static_cast<double>(usedStocks); // Vector division

            double cum = 0.0;
            for (int t = 0; t < T; ++t){
                cum += aar[t];
                caar[t] = cum;
            }

            result.AAR_samples.push_back(aar);
            result.CAAR_samples.push_back(caar);
        }

        return result;
    }

    // Perform bootstrap sampling for all three groups simultaneously
    void Bootstrapper::runBootstrap(const std::vector<Stock>& missGroup,
                                    const std::vector<Stock>& meetGroup,
                                    const std::vector<Stock>& beatGroup,
                                    GroupBootstrapResult& missResult, 
                                    GroupBootstrapResult& meetResult,
                                    GroupBootstrapResult& beatResult)
    {
        missResult = bootstrapSingleGroup(missGroup);
        meetResult = bootstrapSingleGroup(meetGroup);
        beatResult = bootstrapSingleGroup(beatGroup);
    }
}