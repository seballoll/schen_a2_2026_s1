#include "threading/SequentialSolver.h"
#include "threading/FineGrainedSolver.h"
#include "threading/CoarseGrainedSolver.h"
#include "threading/SMTSolver.h"
#include "threading/CMPSolver.h"
#include "metrics/PerformanceMetrics.h"
#include "Config.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string toLower(std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

void printHelp(const char* exe) {
    std::cout
        << "Dummy/partial run for each multithread model (seq/fgmt/cgmt/smt/cmp) with timings.\n\n"
        << "Usage:\n  "
        << exe
        << " [threads] [steps] [particles] [dt] [stage]\n\n"
        << "Args (all optional):\n"
        << "  threads   Threads for parallel models (default: 4)\n"
        << "  steps     Number of iterations to run (default: 200)\n"
        << "  particles Number of particles (default: Config::NUM_PARTICLES)\n"
        << "  dt        Timestep in seconds (default: Config::DT)\n"
        << "  stage     Pipeline cutoff stage (default: forces)\n\n"
        << "Stages:\n"
        << "  neighbours | density | forces | integrate | boundary | full\n\n"
        << "Examples:\n"
        << "  " << exe << " 4 200 2000 0.002 forces\n"
        << "  " << exe << " 8 300 5000 0.001 density\n";
}

SPHSolver::PipelineStage parseStage(const std::string& stageArg) {
    const std::string s = toLower(stageArg);
    if (s == "neigh" || s == "neighbor" || s == "neighbour" || s == "neighbours" || s == "neighbors") {
        return SPHSolver::PipelineStage::Neighbours;
    }
    if (s == "density" || s == "pressure" || s == "densitypressure") {
        return SPHSolver::PipelineStage::DensityPressure;
    }
    if (s == "force" || s == "forces") {
        return SPHSolver::PipelineStage::Forces;
    }
    if (s == "integrate" || s == "integration") {
        return SPHSolver::PipelineStage::Integrate;
    }
    if (s == "boundary" || s == "borders" || s == "walls") {
        return SPHSolver::PipelineStage::Boundary;
    }
    if (s == "full" || s == "pipeline" || s == "all") {
        return SPHSolver::PipelineStage::Boundary;
    }

    throw std::invalid_argument("Unknown stage: " + stageArg);
}

std::string stageName(SPHSolver::PipelineStage stage) {
    switch (stage) {
        case SPHSolver::PipelineStage::Neighbours:      return "neighbours";
        case SPHSolver::PipelineStage::DensityPressure: return "density";
        case SPHSolver::PipelineStage::Forces:          return "forces";
        case SPHSolver::PipelineStage::Integrate:       return "integrate";
        case SPHSolver::PipelineStage::Boundary:        return "full";
    }
    return "full";
}

struct ModelFactory {
    std::string name;
    int threads;
    std::function<std::unique_ptr<SPHSolver>()> create;
};

double runPartial(SPHSolver& solver,
                  int steps,
                  int particles,
                  float dt,
                  SPHSolver::PipelineStage stage,
                  PerformanceMetrics& metrics) {
    solver.initDamBreak(particles, Config::DOMAIN_WIDTH, Config::DOMAIN_HEIGHT);

    metrics.startTimer();
    for (int i = 0; i < steps; ++i) {
        if (stage == SPHSolver::PipelineStage::Boundary) {
            solver.step(dt);
        } else {
            solver.stepUpTo(dt, stage);
        }
    }
    return metrics.stopTimer();
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc >= 2) {
        const std::string arg1 = argv[1];
        if (arg1 == "--help" || arg1 == "-h") {
            printHelp(argv[0]);
            return 0;
        }
    }

    int threads     = (argc >= 2) ? std::stoi(argv[1]) : 4;
    int steps       = (argc >= 3) ? std::stoi(argv[2]) : 200;
    int particles   = (argc >= 4) ? std::stoi(argv[3]) : Config::NUM_PARTICLES;
    float dt        = (argc >= 5) ? std::stof(argv[4]) : Config::DT;
    std::string stg = (argc >= 6) ? argv[5] : std::string{"forces"};

    if (threads < 1) threads = 1;
    if (steps < 1) steps = 1;
    if (particles < 1) particles = 1;

    SPHSolver::PipelineStage stage;
    try {
        stage = parseStage(stg);
    } catch (const std::exception& e) {
        std::cerr << "[sph_dummy] " << e.what() << "\n\n";
        printHelp(argv[0]);
        return 2;
    }

    unsigned int hw = std::thread::hardware_concurrency();

    std::cout << "╔═══════════════════════════════════════════════════════╗\n";
    std::cout << "║   SPH DUMMY/PARTIAL — Per-Model Timing (Headless)     ║\n";
    std::cout << "╠═══════════════════════════════════════════════════════╣\n";
    std::cout << "║  Stage:            " << stageName(stage) << "\n";
    std::cout << "║  Threads (par):    " << threads << "\n";
    std::cout << "║  Particles:        " << particles << "\n";
    std::cout << "║  Steps:            " << steps << "\n";
    std::cout << "║  dt:               " << dt << " s\n";
    std::cout << "║  HW Threads:       " << hw << "\n";
    std::cout << "╚═══════════════════════════════════════════════════════╝\n\n";

    std::vector<ModelFactory> models;
    models.push_back({"Sequential", 1, []() {
                         return std::make_unique<SequentialSolver>();
                     }});

    models.push_back({"FGMT", threads, [threads]() {
                         return std::make_unique<FineGrainedSolver>(threads);
                     }});
    models.push_back({"CGMT", threads, [threads]() {
                         return std::make_unique<CoarseGrainedSolver>(threads);
                     }});
    models.push_back({"SMT", threads, [threads]() {
                         return std::make_unique<SMTSolver>(threads);
                     }});
    models.push_back({"CMP", threads, [threads]() {
                         return std::make_unique<CMPSolver>(threads);
                     }});

    PerformanceMetrics metrics;
    double seqTime = 0.0;

    for (auto& m : models) {
        std::cout << "Running: " << m.name << " (" << m.threads << " threads)..." << std::flush;
        auto solver = m.create();
        const double elapsed = runPartial(*solver, steps, particles, dt, stage, metrics);

        if (m.name == "Sequential") {
            seqTime = elapsed;
            metrics.setBaselineTime(seqTime);
        }

        PerformanceMetrics::RunResult r;
        r.modelName = m.name;
        r.numThreads = m.threads;
        r.numSteps = steps;
        r.numParticles = particles;
        r.totalTimeSec = elapsed;
        r.avgStepTimeMsec = (elapsed / steps) * 1000.0;
        metrics.recordRun(r);

        std::cout << "  " << elapsed << " s\n";
    }

    metrics.printReport();

    return 0;
}
