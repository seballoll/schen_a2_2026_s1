#include "metrics/PerformanceMetrics.h"
#include <fstream>
#include <algorithm>

void PerformanceMetrics::startTimer() {
    start_ = std::chrono::high_resolution_clock::now();
}

double PerformanceMetrics::stopTimer() {
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start_;
    return elapsed.count();
}

void PerformanceMetrics::recordRun(const RunResult& result) {
    results_.push_back(result);
}

void PerformanceMetrics::setBaselineTime(double seqTimeSec) {
    baselineTimeSec_ = seqTimeSec;
}

void PerformanceMetrics::setBaselineCycles(uint64_t seqCycles) {
    baselineCycles_ = seqCycles;
}

double PerformanceMetrics::speedup(double parallelTimeSec) const {
    if (parallelTimeSec <= 0.0) return 0.0;
    return baselineTimeSec_ / parallelTimeSec;
}

double PerformanceMetrics::speedupCycles(uint64_t parallelCycles) const {
    if (baselineCycles_ == 0 || parallelCycles == 0) return 0.0;
    return static_cast<double>(baselineCycles_) / static_cast<double>(parallelCycles);
}

double PerformanceMetrics::efficiency(double parallelTimeSec, int numThreads) const {
    if (numThreads <= 0) return 0.0;
    return speedup(parallelTimeSec) / numThreads;
}

double PerformanceMetrics::efficiencyCycles(uint64_t parallelCycles, int numThreads) const {
    if (numThreads <= 0) return 0.0;
    return speedupCycles(parallelCycles) / numThreads;
}

void PerformanceMetrics::printReport() const {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                        SPH FLUID SIMULATION — PERFORMANCE REPORT                   ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║ " << std::left << std::setw(26) << "Model"
              << std::setw(6)  << "Thr"
              << std::setw(8)  << "Steps"
              << std::setw(11) << "Time(s)"
              << std::setw(10) << "Spd(T)"
              << std::setw(10) << "Eff(T)"
              << std::setw(14) << "SimCycles"
              << std::setw(10) << "Spd(C)"
              << " ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════╣\n";

    for (const auto& r : results_) {
        double spT  = (baselineTimeSec_ > 0.0) ? baselineTimeSec_ / r.totalTimeSec : 1.0;
        double effT = (r.numThreads > 0) ? spT / r.numThreads : spT;
        double spC  = speedupCycles(r.simTotalCycles);

        std::cout << "║ " << std::left  << std::setw(26) << r.modelName
                  << std::setw(6)  << r.numThreads
                  << std::setw(8)  << r.numSteps
                  << std::fixed << std::setprecision(4)
                  << std::setw(11) << r.totalTimeSec
                  << std::setw(10) << spT
                  << std::setw(10) << effT;

        if (r.simTotalCycles > 0 && baselineCycles_ > 0) {
            std::cout << std::setw(14) << r.simTotalCycles
                      << std::setw(10) << spC;
        } else {
            std::cout << std::setw(14) << "-"
                      << std::setw(10) << "-";
        }

        std::cout << " ║\n";
    }

    std::cout << "╚══════════════════════════════════════════════════════════════════════════════════════╝\n\n";
}

void PerformanceMetrics::exportCSV(const std::string& filename) const {
    std::ofstream ofs(filename);
    ofs << "Model,Threads,Steps,Particles,TotalTime_s,AvgStepTime_ms,SpeedupTime,EfficiencyTime,SimTotalCycles,SimTotalTime_s,SpeedupCycles,EfficiencyCycles,SimWorkCycles,SimIdleCycles,SimContextSwitchCycles,SimStallCyclesTotal,SimStallCyclesHidden,SimStallCyclesExposed\n";
    for (const auto& r : results_) {
        double spT  = (baselineTimeSec_ > 0.0) ? baselineTimeSec_ / r.totalTimeSec : 1.0;
        double effT = (r.numThreads > 0) ? spT / r.numThreads : spT;
        double spC  = speedupCycles(r.simTotalCycles);
        double effC = efficiencyCycles(r.simTotalCycles, r.numThreads);
        ofs << r.modelName << ","
            << r.numThreads << ","
            << r.numSteps << ","
            << r.numParticles << ","
            << r.totalTimeSec << ","
            << r.avgStepTimeMsec << ","
            << spT << ","
            << effT << ","
            << r.simTotalCycles << ","
            << r.simTotalTimeSec << ","
            << spC << ","
            << effC << ","
            << r.simWorkCycles << ","
            << r.simIdleCycles << ","
            << r.simContextSwitchCycles << ","
            << r.simStallCyclesTotal << ","
            << r.simStallCyclesHidden << ","
            << r.simStallCyclesExposed << "\n";
    }
    std::cout << "[Metrics] Exported to " << filename << "\n";
}
