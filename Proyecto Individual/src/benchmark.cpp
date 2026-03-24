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
// Usage:  ./sph_benchmark [numSteps] [numParticles] [dt]
// ============================================================

struct ModelFactory {
    std::string name;
    int numThreads;
    std::function<std::unique_ptr<SPHSolver>()> create;
};

double runModel(SPHSolver& solver, int numSteps, int numParticles, float dt, PerformanceMetrics& metrics) {
    solver.initDamBreak(numParticles, Config::DOMAIN_WIDTH, Config::DOMAIN_HEIGHT);

    metrics.startTimer();
    for (int s = 0; s < numSteps; ++s) {
        solver.step(dt);
    }
    return metrics.stopTimer();
}

int main(int argc, char* argv[]) {
    int numSteps     = (argc >= 2) ? std::stoi(argv[1]) : 200;
    int numParticles = (argc >= 3) ? std::stoi(argv[2]) : Config::NUM_PARTICLES;
    float dt         = (argc >= 4) ? std::stof(argv[3]) : Config::DT;

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
    uint64_t seqCycles = 0;

    for (auto& m : models) {
        std::cout << "Running: " << m.name << " (" << m.numThreads << " threads)..." << std::flush;

        auto solver = m.create();
        double elapsed = runModel(*solver, numSteps, numParticles, dt, metrics);
        const auto& ss = solver->simStats();

        if (m.name == "Sequential") {
            seqTime = elapsed;
            metrics.setBaselineTime(seqTime);

            seqCycles = ss.totalCycles;
            metrics.setBaselineCycles(seqCycles);
        }

        PerformanceMetrics::RunResult result;
        result.modelName      = m.name;
        result.numThreads     = m.numThreads;
        result.numSteps       = numSteps;
        result.numParticles   = numParticles;
        result.totalTimeSec   = elapsed;
        result.avgStepTimeMsec = (elapsed / numSteps) * 1000.0;

        // Simulated hardware counters (available for sequential + cooperative FG/CG).
        // SMT/CMP are real multithreading models; we intentionally skip the simulated
        // cycle/stall accounting there to avoid misleading results.
        const bool hasCooperativeCycleModel =
            m.name.rfind("SMT-", 0) != 0 && m.name.rfind("CMP-", 0) != 0;

        if (hasCooperativeCycleModel) {
            result.simTotalCycles = ss.totalCycles;
            result.simTotalTimeSec = solver->simulatedSeconds();
            result.simWorkCycles = ss.workCycles;
            result.simIdleCycles = ss.idleCycles;
            result.simContextSwitchCycles = ss.contextSwitchCycles;
            result.simStallCyclesTotal = ss.stallCyclesTotal;
            result.simStallCyclesHidden = ss.stallCyclesHidden;
            result.simStallCyclesExposed = ss.stallCyclesExposed;
        }
        metrics.recordRun(result);

        std::cout << "  " << elapsed << " s" << std::endl;
    }

    // Print summary
    metrics.printReport();

    // Export CSV
    metrics.exportCSV("benchmark_results.csv");

    return 0;
}
