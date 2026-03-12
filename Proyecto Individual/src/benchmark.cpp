#include "threading/SequentialSolver.h"
#include "threading/FineGrainedSolver.h"
#include "threading/CoarseGrainedSolver.h"
#include "threading/SMTSolver.h"
#include "threading/CMPSolver.h"
#include "metrics/PerformanceMetrics.h"
#include "Config.h"

#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <functional>
#include <algorithm>

// ============================================================
// benchmark — run ALL multithreading models and produce a
// comparison table with metrics:
//   • Execution time
//   • Speedup  S(p) = T_seq / T_par
//   • Parallel efficiency  E(p) = S(p) / p
//   • Scalability across thread counts
//
// Usage:  ./sph_benchmark [numSteps] [numParticles]
// ============================================================

struct ModelFactory {
    std::string name;
    int numThreads;
    std::function<std::unique_ptr<SPHSolver>()> create;
};

double runModel(SPHSolver& solver, int numSteps, PerformanceMetrics& metrics) {
    solver.initDamBreak(Config::NUM_PARTICLES, Config::DOMAIN_WIDTH, Config::DOMAIN_HEIGHT);

    metrics.startTimer();
    for (int s = 0; s < numSteps; ++s) {
        solver.step(Config::DT);
    }
    return metrics.stopTimer();
}

int main(int argc, char* argv[]) {
    int numSteps     = (argc >= 2) ? std::stoi(argv[1]) : 200;
    int numParticles = (argc >= 3) ? std::stoi(argv[2]) : Config::NUM_PARTICLES;

    unsigned int hwThreads = std::thread::hardware_concurrency();
    std::cout << "╔═══════════════════════════════════════════════════════╗\n";
    std::cout << "║     SPH BENCHMARK — Multithreading Model Comparison  ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════╣\n";
    std::cout << "║  Particles:        " << numParticles << std::endl;
    std::cout << "║  Steps:            " << numSteps << std::endl;
    std::cout << "║  HW Threads:       " << hwThreads << std::endl;
    std::cout << "╚═══════════════════════════════════════════════════════╝\n\n";

    // Thread counts to benchmark for scalability
    std::vector<int> threadCounts = {2, 4};
    if (hwThreads >= 8) threadCounts.push_back(8);
    if (hwThreads >= 16) threadCounts.push_back(16);
    threadCounts.push_back(static_cast<int>(hwThreads));

    // Remove duplicates
    std::sort(threadCounts.begin(), threadCounts.end());
    threadCounts.erase(std::unique(threadCounts.begin(), threadCounts.end()), threadCounts.end());

    // Build model list
    std::vector<ModelFactory> models;

    // 1. Sequential baseline
    models.push_back({"Sequential", 1, []() {
        return std::make_unique<SequentialSolver>();
    }});

    // 2-5. Parallel models at various thread counts
    for (int t : threadCounts) {
        models.push_back({"FGMT-" + std::to_string(t), t, [t]() {
            return std::make_unique<FineGrainedSolver>(t);
        }});
        models.push_back({"CGMT-" + std::to_string(t), t, [t]() {
            return std::make_unique<CoarseGrainedSolver>(t);
        }});
        models.push_back({"SMT-" + std::to_string(t), t, [t]() {
            return std::make_unique<SMTSolver>(t);
        }});
        models.push_back({"CMP-" + std::to_string(t), t, [t]() {
            return std::make_unique<CMPSolver>(t);
        }});
    }

    PerformanceMetrics metrics;
    double seqTime = 0.0;

    for (auto& m : models) {
        std::cout << "Running: " << m.name << " (" << m.numThreads << " threads)..." << std::flush;

        auto solver = m.create();
        double elapsed = runModel(*solver, numSteps, metrics);

        if (m.name == "Sequential") {
            seqTime = elapsed;
            metrics.setBaselineTime(seqTime);
        }

        PerformanceMetrics::RunResult result;
        result.modelName      = m.name;
        result.numThreads     = m.numThreads;
        result.numSteps       = numSteps;
        result.numParticles   = numParticles;
        result.totalTimeSec   = elapsed;
        result.avgStepTimeMsec = (elapsed / numSteps) * 1000.0;
        metrics.recordRun(result);

        std::cout << "  " << elapsed << " s" << std::endl;
    }

    // Print summary
    metrics.printReport();

    // Export CSV
    metrics.exportCSV("benchmark_results.csv");

    return 0;
}
