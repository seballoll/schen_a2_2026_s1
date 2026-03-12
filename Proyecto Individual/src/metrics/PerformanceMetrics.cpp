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

double PerformanceMetrics::speedup(double parallelTimeSec) const {
    if (parallelTimeSec <= 0.0) return 0.0;
    return baselineTimeSec_ / parallelTimeSec;
}

double PerformanceMetrics::efficiency(double parallelTimeSec, int numThreads) const {
    if (numThreads <= 0) return 0.0;
    return speedup(parallelTimeSec) / numThreads;
}

void PerformanceMetrics::printReport() const {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                        SPH FLUID SIMULATION — PERFORMANCE REPORT                   ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════╣\n";
    std::cout << "║ " << std::left << std::setw(30) << "Model"
              << std::setw(8)  << "Thr"
              << std::setw(10) << "Steps"
              << std::setw(12) << "Time (s)"
              << std::setw(12) << "ms/step"
              << std::setw(10) << "Speedup"
              << std::setw(10) << "Effic."
              << " ║\n";
    std::cout << "╠══════════════════════════════════════════════════════════════════════════════════════╣\n";

    for (const auto& r : results_) {
        double sp  = (baselineTimeSec_ > 0.0) ? baselineTimeSec_ / r.totalTimeSec : 1.0;
        double eff = (r.numThreads > 0) ? sp / r.numThreads : sp;

        std::cout << "║ " << std::left  << std::setw(30) << r.modelName
                  << std::setw(8)  << r.numThreads
                  << std::setw(10) << r.numSteps
                  << std::fixed << std::setprecision(4)
                  << std::setw(12) << r.totalTimeSec
                  << std::setw(12) << r.avgStepTimeMsec
                  << std::setw(10) << sp
                  << std::setw(10) << eff
                  << " ║\n";
    }

    std::cout << "╚══════════════════════════════════════════════════════════════════════════════════════╝\n\n";
}

void PerformanceMetrics::exportCSV(const std::string& filename) const {
    std::ofstream ofs(filename);
    ofs << "Model,Threads,Steps,Particles,TotalTime_s,AvgStepTime_ms,Speedup,Efficiency\n";
    for (const auto& r : results_) {
        double sp  = (baselineTimeSec_ > 0.0) ? baselineTimeSec_ / r.totalTimeSec : 1.0;
        double eff = (r.numThreads > 0) ? sp / r.numThreads : sp;
        ofs << r.modelName << ","
            << r.numThreads << ","
            << r.numSteps << ","
            << r.numParticles << ","
            << r.totalTimeSec << ","
            << r.avgStepTimeMsec << ","
            << sp << ","
            << eff << "\n";
    }
    std::cout << "[Metrics] Exported to " << filename << "\n";
}
