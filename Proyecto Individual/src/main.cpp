#include "core/SPHSolver.h"
#include "threading/SequentialSolver.h"
#include "threading/FineGrainedSolver.h"
#include "threading/CoarseGrainedSolver.h"
#include "threading/SMTSolver.h"
#include "threading/CMPSolver.h"
#include "metrics/PerformanceMetrics.h"
#include "visualization/Renderer.h"
#include "Config.h"

#include <iostream>
#include <memory>
#include <string>
#include <chrono>

// ============================================================
// main — interactive simulation with SFML visualisation
//
// Usage:  ./sph_simulation [model] [threads] [particles] [dt]
//   model:   seq | fgmt | cgmt | smt | cmp   (default: seq)
//   threads: number of threads for parallel models (default: 4)
//   particles: number of particles (default: Config::NUM_PARTICLES)
//   dt: timestep in seconds (default: Config::DT)
//
// Examples:
//   ./sph_simulation              # Sequential (1 thread)
//   ./sph_simulation seq          # Sequential explicitly
//   ./sph_simulation fgmt 4       # Fine-Grained MT, 4 threads
//   ./sph_simulation cgmt 8       # Coarse-Grained MT, 8 threads
//   ./sph_simulation smt 4        # SMT (HyperThreading), 4 threads
//   ./sph_simulation cmp 4        # CMP (Chip Multiprocessing), 4 threads
// ============================================================

static void printHelp() {
    std::cout << R"(
╔══════════════════════════════════════════════════════════════╗
║               SPH FLUID SIMULATION — HELP                   ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║  Usage:  ./sph_simulation [model] [threads] [particles] [dt]  ║
║                                                              ║
║  Models:                                                     ║
║    seq   — Sequential (single-threaded baseline)             ║
║    fgmt  — Fine-Grained Multithreading                       ║
║    cgmt  — Coarse-Grained Multithreading                     ║
║    smt   — SMT / HyperThreading (pin to logical cores)       ║
║    cmp   — CMP / Chip Multiprocessing (pin to phys. cores)   ║
║                                                              ║
║  Threads:    1-N (default: 4, ignored for 'seq')             ║
║  Particles:  number of particles (default: 2000)             ║
║  dt:         timestep in seconds (default: Config::DT)        ║
║                                                              ║
║  Examples:                                                   ║
║    ./sph_simulation              # Sequential, 2000 particles║
║    ./sph_simulation fgmt 4       # FGMT with 4 threads       ║
║    ./sph_simulation cmp 8 5000   # CMP, 8 threads, 5000 part.║
║    ./sph_simulation fgmt 6 2000 0.002  # smaller dt (slower)  ║
║                                                              ║
║  Visual comparison (all models side-by-side):                ║
║    ./sph_compare [threads]       # 2x2 split-screen          ║
║                                                              ║
║  Headless benchmark (all models, metrics table + CSV):       ║
║    ./sph_benchmark [steps] [particles]                       ║
║                                                              ║
╚══════════════════════════════════════════════════════════════╝
)" << std::endl;
}

int main(int argc, char* argv[]) {
    // Check for --help / -h
    if (argc >= 2) {
        std::string arg1 = argv[1];
        if (arg1 == "--help" || arg1 == "-h" || arg1 == "help") {
            printHelp();
            return 0;
        }
    }

    // Parse arguments
    std::string modelArg = (argc >= 2) ? argv[1] : "seq";
    int numThreads       = (argc >= 3) ? std::stoi(argv[2]) : Config::DEFAULT_THREAD_COUNT;
    int numParticles     = (argc >= 4) ? std::stoi(argv[3]) : Config::NUM_PARTICLES;
    float dt             = (argc >= 5) ? std::stof(argv[4]) : Config::DT;

    // Create solver based on model choice
    std::unique_ptr<SPHSolver> solver;

    if (modelArg == "fgmt") {
        solver = std::make_unique<FineGrainedSolver>(numThreads);
    } else if (modelArg == "cgmt") {
        solver = std::make_unique<CoarseGrainedSolver>(numThreads);
    } else if (modelArg == "smt") {
        solver = std::make_unique<SMTSolver>(numThreads);
    } else if (modelArg == "cmp") {
        solver = std::make_unique<CMPSolver>(numThreads);
    } else {
        solver = std::make_unique<SequentialSolver>();
        numThreads = 1;
    }

    std::cout << "╔═══════════════════════════════════════════╗" << std::endl;
    std::cout << "║       SPH Fluid Simulation — Visual       ║" << std::endl;
    std::cout << "╠═══════════════════════════════════════════╣" << std::endl;
    std::cout << "║  Model:      " << solver->modelName() << std::endl;
    std::cout << "║  Threads:    " << numThreads << std::endl;
    std::cout << "║  Particles:  " << numParticles << std::endl;
    std::cout << "║  Timestep:   " << dt << " s" << std::endl;
    std::cout << "║  Domain:     " << Config::DOMAIN_WIDTH << " x " << Config::DOMAIN_HEIGHT << std::endl;
    std::cout << "╚═══════════════════════════════════════════╝" << std::endl;
    std::cout << "\nClose the window to stop the simulation.\n" << std::endl;

    // Initialise particles
    solver->initDamBreak(numParticles, Config::DOMAIN_WIDTH, Config::DOMAIN_HEIGHT);

    // Renderer
    Renderer renderer;

    // Metrics
    PerformanceMetrics metrics;
    metrics.startTimer();

    int step = 0;
    double msPerStep = 0.0;
    auto stepStart = std::chrono::high_resolution_clock::now();

    while (renderer.isOpen() && step < Config::MAX_STEPS) {
        renderer.handleEvents();

        auto t0 = std::chrono::high_resolution_clock::now();
    solver->step(dt);
        auto t1 = std::chrono::high_resolution_clock::now();
        ++step;

        // Exponential moving average for smooth ms/step display
        double thisMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
        msPerStep = (step == 1) ? thisMs : msPerStep * 0.95 + thisMs * 0.05;

        // Draw with HUD
        renderer.draw(solver->particles(),
                      Config::DOMAIN_WIDTH, Config::DOMAIN_HEIGHT,
                      step, solver->modelName(),
                      msPerStep, numThreads);
    }

    double elapsed = metrics.stopTimer();
    std::cout << "\n--- Simulation Complete ---" << std::endl;
    std::cout << "Steps:     " << step << std::endl;
    std::cout << "Total:     " << elapsed << " s" << std::endl;
    std::cout << "Avg step:  " << (elapsed / step) * 1000.0 << " ms" << std::endl;

    return 0;
}
