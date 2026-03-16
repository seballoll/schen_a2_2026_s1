#include "core/SPHSolver.h"
#include "threading/SequentialSolver.h"
#include "threading/FineGrainedSolver.h"
#include "threading/CoarseGrainedSolver.h"
#include "threading/SMTSolver.h"
#include "threading/CMPSolver.h"
#include "visualization/Renderer.h"
#include "Config.h"

#include <SFML/Graphics.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>

// ============================================================
// visual_compare — run up to 4 models side-by-side in a
// split-screen window so you can SEE the differences.
//
// All models start from the SAME initial condition and run
// in lock-step (same timestep each frame).
//
// Layout:  2×2 grid  (or 1×2 / 1×3 if fewer models selected)
//
// Usage:
//   ./sph_compare [threads] [particles] [dt] [models...]
//
// Examples:
//   ./sph_compare 4                      # default: all models, 2000 particles
//   ./sph_compare 4 5000                 # all models, 5000 particles
//   ./sph_compare 4 2000 0.002           # all models, 2000 particles, dt=0.002
//   ./sph_compare 4 2000 0.002 seq fgmt cmp   # these 3, dt=0.002
//   ./sph_compare 8 3000 0.001 fgmt cgmt      # FGMT vs CGMT, dt=0.001
// ============================================================

struct SimPanel {
    std::string label;
    std::unique_ptr<SPHSolver> solver;
    double msPerStep = 0.0;
};

int main(int argc, char* argv[]) {
    int numThreads   = (argc >= 2) ? std::stoi(argv[1]) : Config::DEFAULT_THREAD_COUNT;
    int numParticles = (argc >= 3) ? std::stoi(argv[2]) : Config::NUM_PARTICLES;
    float dt         = (argc >= 4) ? std::stof(argv[3]) : Config::DT;

    // Collect model names from args (starting at arg 4), or use defaults
    std::vector<std::string> modelNames;
    if (argc >= 5) {
        for (int i = 4; i < argc; ++i)
            modelNames.push_back(argv[i]);
    } else {
        modelNames = {"seq", "fgmt", "cgmt", "smt", "cmp"};
    }

    // Build solvers
    std::vector<SimPanel> panels;
    for (const auto& name : modelNames) {
        SimPanel p;
        if (name == "fgmt") {
            p.label = "FGMT (" + std::to_string(numThreads) + "T)";
            p.solver = std::make_unique<FineGrainedSolver>(numThreads);
        } else if (name == "cgmt") {
            p.label = "CGMT (" + std::to_string(numThreads) + "T)";
            p.solver = std::make_unique<CoarseGrainedSolver>(numThreads);
        } else if (name == "smt") {
            p.label = "SMT (" + std::to_string(numThreads) + "T)";
            p.solver = std::make_unique<SMTSolver>(numThreads);
        } else if (name == "cmp") {
            p.label = "CMP (" + std::to_string(numThreads) + "T)";
            p.solver = std::make_unique<CMPSolver>(numThreads);
        } else {
            p.label = "Sequential (1T)";
            p.solver = std::make_unique<SequentialSolver>();
        }
        p.solver->initDamBreak(numParticles, Config::DOMAIN_WIDTH, Config::DOMAIN_HEIGHT);
        panels.push_back(std::move(p));
    }

    int N = static_cast<int>(panels.size());
    if (N == 0) {
        std::cerr << "No models selected." << std::endl;
        return 1;
    }

    // Determine grid layout
    int cols, rows;
    if (N <= 2)      { cols = N; rows = 1; }
    else if (N <= 4) { cols = 2; rows = 2; }
    else if (N <= 6) { cols = 3; rows = 2; }
    else             { cols = 3; rows = 3; }

    // Window size: bigger for comparison
    int winW = Config::WINDOW_WIDTH;
    int winH = Config::WINDOW_HEIGHT;

    float cellW = static_cast<float>(winW) / cols;
    float cellH = static_cast<float>(winH) / rows;

    std::cout << "╔══════════════════════════════════════════════════════╗" << std::endl;
    std::cout << "║     SPH Visual Comparison — Side by Side            ║" << std::endl;
    std::cout << "╠══════════════════════════════════════════════════════╣" << std::endl;
    std::cout << "║  Models: " << N << "  |  Grid: " << cols << "×" << rows
              << "  |  Threads: " << numThreads
              << "  |  Particles: " << numParticles << std::endl;
    for (int i = 0; i < N; ++i)
        std::cout << "║  [" << (i+1) << "] " << panels[i].label << std::endl;
    std::cout << "╚══════════════════════════════════════════════════════╝" << std::endl;
    std::cout << "\nClose the window to stop.\n" << std::endl;

    // Create renderer (we'll manually control the window)
    Renderer renderer;
    auto& win = renderer.window();

    int step = 0;
    while (renderer.isOpen() && step < Config::MAX_STEPS) {
        renderer.handleEvents();

        // Step all models
        for (auto& panel : panels) {
            auto t0 = std::chrono::high_resolution_clock::now();
            panel.solver->step(dt);
            auto t1 = std::chrono::high_resolution_clock::now();
            double thisMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
            panel.msPerStep = (step == 0) ? thisMs : panel.msPerStep * 0.9 + thisMs * 0.1;
        }
        ++step;

        // Draw
        win.clear(sf::Color(15, 15, 20));

        for (int i = 0; i < N; ++i) {
            int col = i % cols;
            int row = i / cols;
            float x = col * cellW;
            float y = row * cellH;

            // Leave 1px gap between panels
            sf::FloatRect viewport(x + 1, y + 1, cellW - 2, cellH - 2);

            renderer.drawRegion(panels[i].solver->particles(),
                                Config::DOMAIN_WIDTH, Config::DOMAIN_HEIGHT,
                                viewport, panels[i].label,
                                panels[i].msPerStep);
        }

        // Step counter at bottom
        if (renderer.font().getInfo().family.size() > 0) {
            std::ostringstream oss;
            oss << "Step " << step << "  |  dt=" << dt << "s";
            sf::Text stepText;
            stepText.setFont(renderer.font());
            stepText.setCharacterSize(13);
            stepText.setFillColor(sf::Color(120, 120, 130));
            stepText.setPosition(8, winH - 20.0f);
            stepText.setString(oss.str());
            win.draw(stepText);
        }

        // Grid separator lines
        sf::RectangleShape line;
        line.setFillColor(sf::Color(50, 90, 160));

        // Vertical separators
        for (int c = 1; c < cols; ++c) {
            line.setSize(sf::Vector2f(2, static_cast<float>(winH)));
            line.setPosition(c * cellW - 1, 0);
            win.draw(line);
        }
        // Horizontal separators
        for (int r = 1; r < rows; ++r) {
            line.setSize(sf::Vector2f(static_cast<float>(winW), 2));
            line.setPosition(0, r * cellH - 1);
            win.draw(line);
        }

        win.display();
    }

    // Print final timing summary
    std::cout << "\n--- Comparison Complete (" << step << " steps) ---\n" << std::endl;
    std::cout << "  Model                     Avg ms/step" << std::endl;
    std::cout << "  ──────────────────────────────────────" << std::endl;
    for (const auto& p : panels) {
        std::cout << "  " << std::left << std::setw(28) << p.label
                  << std::fixed << std::setprecision(2) << p.msPerStep << std::endl;
    }
    std::cout << std::endl;

    return 0;
}
