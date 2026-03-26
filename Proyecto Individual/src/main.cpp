#include "simulation/SequentialStageRunner.h"

#include <iomanip>
#include <iostream>

namespace {

void printInitialParticleSample(const sim::SPHState& state) {
    const auto& particles = state.particles;
    std::cout << "Initial particle sample (first 3):\n";

    for (int i = 0; i < 3 && i < static_cast<int>(particles.size()); ++i) {
        const auto& p = particles[static_cast<std::size_t>(i)];
        std::cout << "  p" << i
                  << " pos=(" << p.position.x << ", " << p.position.y << ")"
                  << " vel=(" << p.velocity.x << ", " << p.velocity.y << ")"
                  << " rho=" << p.density
                  << " P=" << p.pressure
                  << "\n";
    }
}

void printRunSummary(const sim::SequentialStageRunner& runner, const sim::RunConfig& cfg) {
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

    for (const auto& stageMetrics : run.perStage) {
        std::cout << std::left
                  << std::setw(20) << stageMetrics.stageName
                  << std::setw(12) << std::fixed << std::setprecision(3) << stageMetrics.wallMs
                  << std::setw(14) << stageMetrics.workCycles
                  << std::setw(14) << stageMetrics.idleCycles
                  << std::setw(14) << stageMetrics.totalCycles
                  << "\n";
    }
}

void printPostRunDensityPressureSample(const sim::SPHState& state) {
    const auto& particles = state.particles;
    std::cout << "\nPost-run density/pressure/force sample (first 3):\n";
    for (int i = 0; i < 3 && i < static_cast<int>(particles.size()); ++i) {
        const auto& p = particles[static_cast<std::size_t>(i)];
        std::cout << "  p" << i
                  << " rho=" << p.density
                  << " P=" << p.pressure
                  << " F=(" << p.force.x << ", " << p.force.y << ")"
                  << "\n";
    }
}

} // namespace

int main(int argc, char* argv[]) {
    sim::RunConfig cfg;
    cfg.steps = (argc >= 2) ? std::stoi(argv[1]) : 120;
    cfg.particles = (argc >= 3) ? std::stoi(argv[2]) : 2000;
    cfg.threads = 1;
    cfg.dt = 0.003f;
    cfg.seed = 20260324ULL;

    sim::SequentialStageRunner runner;
    runner.initialize(cfg);
    printInitialParticleSample(runner.state());

    runner.runAllSteps();
    printRunSummary(runner, cfg);
    printPostRunDensityPressureSample(runner.state());

    return 0;
}
