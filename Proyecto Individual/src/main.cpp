#include "simulation/SequentialStageRunner.h"
#include "threads/CMPThreadStrategy.h"
#include "threads/FGMTRoundRobinStrategy.h"
#include "threads/ParallelChunkedThreadStrategy.h"
#include "threads/SequentialThreadStrategy.h"
#include "threads/SMTThreadStrategy.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>

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
              << std::setw(14) << "CtxSwitch"
              << std::setw(14) << "HiddenStall"
              << std::setw(14) << "ExposedStl"
              << std::setw(14) << "TotalCycles"
              << "\n";

    for (const auto& stageMetrics : run.perStage) {
        std::cout << std::left
                  << std::setw(20) << stageMetrics.stageName
                  << std::setw(12) << std::fixed << std::setprecision(3) << stageMetrics.wallMs
                  << std::setw(14) << stageMetrics.workCycles
                  << std::setw(14) << stageMetrics.idleCycles
                  << std::setw(14) << stageMetrics.contextSwitchCycles
                  << std::setw(14) << stageMetrics.stallHiddenCycles
                  << std::setw(14) << stageMetrics.stallExposedCycles
                  << std::setw(14) << stageMetrics.totalCycles
                  << "\n";
    }
}

void printPostRunDensityPressureSample(const sim::SPHState& state) {
    const auto& particles = state.particles;
    std::cout << "\nPost-run sample (first 3):\n";
    for (int i = 0; i < 3 && i < static_cast<int>(particles.size()); ++i) {
        const auto& p = particles[static_cast<std::size_t>(i)];
        std::cout << "  p" << i
                  << " pos=(" << p.position.x << ", " << p.position.y << ")"
                  << " vel=(" << p.velocity.x << ", " << p.velocity.y << ")"
                  << " rho=" << p.density
                  << " P=" << p.pressure
                  << " F=(" << p.force.x << ", " << p.force.y << ")"
                  << "\n";
    }
}

void printLastStepMetrics(const sim::SequentialStageRunner& runner) {
    const auto& steps = runner.stepSnapshots();
    if (steps.empty()) {
        std::cout << "\nNo step metrics snapshots available.\n";
        return;
    }

    const auto& last = steps.back();
    std::cout << "\nStage 6 summary (last step):\n";
    std::cout << "  step=" << last.stepIndex
              << " avgDensity=" << std::fixed << std::setprecision(3) << last.avgDensity
              << " avgSpeed=" << std::fixed << std::setprecision(3) << last.avgSpeed
              << " maxSpeed=" << std::fixed << std::setprecision(3) << last.maxSpeedObserved
              << " kinetic=" << std::fixed << std::setprecision(3) << last.totalKineticEnergy
              << " invalid=" << last.invalidParticleCount
              << "\n";
}

void exportStepMetricsCsv(const sim::SequentialStageRunner& runner, const std::string& filePath) {
    std::ofstream out(filePath);
    out << "Step,AvgDensity,AvgSpeed,MaxSpeed,TotalKineticEnergy,InvalidParticleCount\n";

    for (const auto& s : runner.stepSnapshots()) {
        out << s.stepIndex << ","
            << s.avgDensity << ","
            << s.avgSpeed << ","
            << s.maxSpeedObserved << ","
            << s.totalKineticEnergy << ","
            << s.invalidParticleCount << "\n";
    }

    std::cout << "Exported step metrics to: " << filePath << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
    sim::RunConfig cfg;
    cfg.steps = (argc >= 2) ? std::stoi(argv[1]) : 120;
    cfg.particles = (argc >= 3) ? std::stoi(argv[2]) : 2000;
    cfg.threads = (argc >= 4) ? std::stoi(argv[3]) : 1;
    cfg.dt = 0.003f;
    cfg.seed = 20260324ULL;

    std::string strategyArg = (argc >= 5) ? argv[4] : "sequential";
    const int fgmtQuantum = (argc >= 6) ? std::stoi(argv[5]) : 16;

    // Optional deterministic seed argument:
    // - For non-FGMT: argv[5] is seed.
    // - For FGMT: argv[5] is quantum and argv[6] is seed.
    if (strategyArg == "fgmt") {
        if (argc >= 7) {
            cfg.seed = static_cast<sim::CycleCount>(std::stoull(argv[6]));
        }
    } else {
        if (argc >= 6) {
            cfg.seed = static_cast<sim::CycleCount>(std::stoull(argv[5]));
        }
    }

    std::unique_ptr<th::ThreadStrategyContract> strategy;
    if (strategyArg == "chunked") {
        strategy = std::make_unique<th::ParallelChunkedThreadStrategy>();
    } else if (strategyArg == "fgmt") {
        strategy = std::make_unique<th::FGMTRoundRobinStrategy>(fgmtQuantum);
    } else if (strategyArg == "smt") {
        strategy = std::make_unique<th::SMTThreadStrategy>();
    } else if (strategyArg == "cmp") {
        strategy = std::make_unique<th::CMPThreadStrategy>();
    } else {
        strategy = std::make_unique<th::SequentialThreadStrategy>();
        strategyArg = "sequential";
    }

    sim::SequentialStageRunner runner(std::move(strategy));
    runner.initialize(cfg);
    std::cout << "Execution strategy: " << strategyArg << " (threads=" << cfg.threads << ")\n";
    if (strategyArg == "fgmt") {
        std::cout << "FGMT simulated quantum (items): " << fgmtQuantum << "\n";
    }
    printInitialParticleSample(runner.state());

    runner.runAllSteps();
    printRunSummary(runner, cfg);
    printPostRunDensityPressureSample(runner.state());
    printLastStepMetrics(runner);
    exportStepMetricsCsv(runner, "stage6_step_metrics.csv");

    return 0;
}
