#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <iostream>
#include <iomanip>
#include <cstdint>

// ============================================================
// PerformanceMetrics — measures and reports:
//   • Execution time
//   • Speedup  S(p) = T_seq / T_parallel
//   • Parallel efficiency  E(p) = S(p) / p
//   • Scalability table for different thread counts
// ============================================================
class PerformanceMetrics {
public:
    struct RunResult {
        std::string modelName;
        int         numThreads;
        int         numSteps;
        int         numParticles;
        double      totalTimeSec;          // wall-clock seconds
        double      avgStepTimeMsec;       // ms per step

        // Simulated hardware counters (optional; 0 if not collected)
        uint64_t    simTotalCycles = 0;
        double      simTotalTimeSec = 0.0; // simTotalCycles / CPU_CLOCK_HZ
        uint64_t    simWorkCycles = 0;
        uint64_t    simIdleCycles = 0;
        uint64_t    simContextSwitchCycles = 0;
        uint64_t    simStallCyclesTotal = 0;
        uint64_t    simStallCyclesHidden = 0;
        uint64_t    simStallCyclesExposed = 0;
    };

    /// Start a timer.
    void startTimer();
    /// Stop the timer and return elapsed seconds.
    double stopTimer();

    /// Record a completed run.
    void recordRun(const RunResult& result);

    /// Set the sequential baseline time (needed for speedup).
    void setBaselineTime(double seqTimeSec);

    /// Set sequential baseline cycles (needed for simulated speedup).
    void setBaselineCycles(uint64_t seqCycles);

    /// Print a formatted comparison table to stdout.
    void printReport() const;

    /// Export results to CSV.
    void exportCSV(const std::string& filename) const;

    /// Get speedup relative to baseline.
    double speedup(double parallelTimeSec) const;
    double speedupCycles(uint64_t parallelCycles) const;
    /// Get parallel efficiency.
    double efficiency(double parallelTimeSec, int numThreads) const;
    double efficiencyCycles(uint64_t parallelCycles, int numThreads) const;

private:
    std::chrono::high_resolution_clock::time_point start_;
    double baselineTimeSec_ = 0.0;
    uint64_t baselineCycles_ = 0;
    std::vector<RunResult> results_;
};
