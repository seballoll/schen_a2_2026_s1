#include "core/SPHSolver.h"
#include "threading/SequentialSolver.h"
#include "threading/FineGrainedSolver.h"
#include "threading/CoarseGrainedSolver.h"
#include "threading/SMTSolver.h"
#include "threading/CMPSolver.h"
#include "metrics/PerformanceMetrics.h"
#include "Config.h"

#include <iostream>
#include <memory>
#include <string>

// ============================================================
// main_headless — runs simulation without visualisation
// (used when SFML is not available)
// ============================================================

int main(int argc, char* argv[]) {
    std::string modelArg = (argc >= 2) ? argv[1] : "seq";
    int numThreads       = (argc >= 3) ? std::stoi(argv[2]) : Config::DEFAULT_THREAD_COUNT;
    int numSteps         = (argc >= 4) ? std::stoi(argv[3]) : 500;
    float dt             = (argc >= 5) ? std::stof(argv[4]) : Config::DT;

    std::unique_ptr<SPHSolver> solver;
    if (modelArg == "fgmt")      solver = std::make_unique<FineGrainedSolver>(numThreads);
    else if (modelArg == "cgmt") solver = std::make_unique<CoarseGrainedSolver>(numThreads);
    else if (modelArg == "smt")  solver = std::make_unique<SMTSolver>(numThreads);
    else if (modelArg == "cmp")  solver = std::make_unique<CMPSolver>(numThreads);
    else { solver = std::make_unique<SequentialSolver>(); numThreads = 1; }

    std::cout << "=== SPH Fluid Simulation (Headless) ===" << std::endl;
    std::cout << "Model:      " << solver->modelName() << std::endl;
    std::cout << "Particles:  " << Config::NUM_PARTICLES << std::endl;
    std::cout << "Steps:      " << numSteps << std::endl;
    std::cout << "dt:         " << dt << std::endl;

    solver->initDamBreak(Config::NUM_PARTICLES, Config::DOMAIN_WIDTH, Config::DOMAIN_HEIGHT);

    PerformanceMetrics metrics;
    metrics.startTimer();

    for (int s = 0; s < numSteps; ++s) {
    solver->step(dt);
        if ((s + 1) % 100 == 0)
            std::cout << "  Step " << (s + 1) << "/" << numSteps << std::endl;
    }

    double elapsed = metrics.stopTimer();
    std::cout << "\nTotal:     " << elapsed << " s" << std::endl;
    std::cout << "Avg step:  " << (elapsed / numSteps) * 1000.0 << " ms" << std::endl;

    return 0;
}
