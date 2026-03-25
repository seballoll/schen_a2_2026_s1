#include "simulation/SequentialStageRunner.h"

#include <iomanip>
#include <iostream>

int main(int argc, char* argv[]) {
    sim::RunConfig cfg;
    cfg.steps = (argc >= 2) ? std::stoi(argv[1]) : 120;
    cfg.particles = (argc >= 3) ? std::stoi(argv[2]) : 2000;
    cfg.threads = 1;
    cfg.dt = 0.003f;
    cfg.seed = 20260324ULL;

    sim::SequentialStageRunner runner;
    runner.initialize(cfg);
    runner.runAllSteps();

    const auto& run = runner.runMetrics();

    std::cout << "\n=== StageRunner Baseline ===\n";
    std::cout << "Model: " << runner.modelName() << "\n";
    std::cout << "Steps: " << cfg.steps << "  Particles: " << cfg.particles << "\n";
    std::cout << "Total wall (ms): " << std::fixed << std::setprecision(3)
              << run.totalWallMs << "\n";
    std::cout << "Total cycles: " << run.totalCycles << "\n\n";

    std::cout << std::left
              << std::setw(20) << "Stage"
              << std::setw(12) << "Wall(ms)"
              << std::setw(14) << "WorkCycles"
              << std::setw(14) << "IdleCycles"
              << std::setw(14) << "TotalCycles"
              << "\n";

    for (const auto& m : run.perStage) {
        std::cout << std::left
                  << std::setw(20) << m.stageName
                  << std::setw(12) << std::fixed << std::setprecision(3) << m.wallMs
                  << std::setw(14) << m.workCycles
                  << std::setw(14) << m.idleCycles
                  << std::setw(14) << m.totalCycles
                  << "\n";
    }

    return 0;
}
