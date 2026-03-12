#include "threading/CoarseGrainedSolver.h"
#include "core/Kernels.h"
#include "Config.h"
#include <functional>
#include <vector>
#include <barrier>

// ============================================================
// Coarse-Grained Multithreading (CGMT)
//
// Strategy: launch persistent worker threads that execute ALL
// pipeline stages.  Between stages, threads hit a barrier
// (modelling a "long-latency stall" context switch).  Each
// thread processes a LARGE contiguous chunk of particles,
// reducing synchronisation overhead compared to FGMT.
// ============================================================

CoarseGrainedSolver::CoarseGrainedSolver(int numThreads)
    : numThreads_(numThreads) {}

std::string CoarseGrainedSolver::modelName() const {
    return "Coarse-Grained MT (" + std::to_string(numThreads_) + " threads)";
}

void CoarseGrainedSolver::parallelFor(int n, std::function<void(int, int)> func) {
    std::vector<std::thread> threads;
    int chunkSize = (n + numThreads_ - 1) / numThreads_;
    for (int t = 0; t < numThreads_; ++t) {
        int start = t * chunkSize;
        int end   = std::min(start + chunkSize, n);
        if (start >= n) break;
        threads.emplace_back(func, start, end);
    }
    for (auto& th : threads) th.join();
}

// ============================================================
// stepParallel — persistent threads walk through all stages
// with std::barrier synchronisation (C++20).
//
// This is the CGMT-specific approach.  The base step() still
// calls the virtual overrides below, which also work.
// ============================================================
void CoarseGrainedSolver::stepParallel(float dt) {
    const int n = static_cast<int>(particles_.size());

    // Stage 1: build spatial hash (single-threaded — hash map not thread-safe)
    buildNeighbourStructure();

    // Stages 2–5 run on persistent threads with barriers
    const int actualThreads = std::min(numThreads_, n);
    std::barrier syncBarrier(actualThreads);

    auto workerFunc = [&](int tid) {
        int chunkSize = (n + actualThreads - 1) / actualThreads;
        int start     = tid * chunkSize;
        int end       = std::min(start + chunkSize, n);
        if (start >= n) return;

        const float h     = Config::SMOOTHING_RADIUS;
        const float mass  = Config::PARTICLE_MASS;
        const float rho0  = Config::REST_DENSITY;
        const float k     = Config::GAS_CONSTANT;
        const float mu    = Config::VISCOSITY;
        const float damp  = Config::BOUNDARY_DAMPING;
        const float domW  = Config::DOMAIN_WIDTH;
        const float domH  = Config::DOMAIN_HEIGHT;

        std::vector<int> neighbours;

        // --- Stage 2: Density & Pressure ---
        for (int i = start; i < end; ++i) {
            Particle& pi = particles_[i];
            grid_.queryNeighbours(pi.position, h, particles_, neighbours);
            float density = 0.0f;
            for (int j : neighbours) {
                float r = (pi.position - particles_[j].position).length();
                density += mass * Kernels::poly6(r, h);
            }
            pi.density  = std::max(density, rho0 * 0.01f);
            pi.pressure = k * (pi.density - rho0);
        }
        syncBarrier.arrive_and_wait(); // ── BARRIER (coarse-grain switch) ──

        // --- Stage 3: Forces ---
        for (int i = start; i < end; ++i) {
            Particle& pi = particles_[i];
            Vec2 fP{0,0}, fV{0,0};
            grid_.queryNeighbours(pi.position, h, particles_, neighbours);
            for (int j : neighbours) {
                if (j == i) continue;
                const Particle& pj = particles_[j];
                Vec2  rij = pi.position - pj.position;
                float r   = rij.length();
                if (r < 1e-6f || r > h) continue;
                float pAvg = (pi.pressure + pj.pressure) / (2.0f * pj.density);
                fP += Kernels::spikyGrad(rij, r, h) * (-mass * pAvg);
                float lapW = Kernels::viscosityLaplacian(r, h);
                fV += (pj.velocity - pi.velocity) * (mu * mass * lapW / pj.density);
            }
            pi.force = fP + fV + Vec2{0.0f, Config::GRAVITY * pi.density};
        }
        syncBarrier.arrive_and_wait(); // ── BARRIER ──

        // --- Stage 4: Integration ---
        for (int i = start; i < end; ++i) {
            Particle& p = particles_[i];
            Vec2 acc = p.force / p.density;
            p.velocity += acc * dt;
            p.position += p.velocity * dt;
        }
        syncBarrier.arrive_and_wait(); // ── BARRIER ──

        // --- Stage 5: Boundary ---
        for (int i = start; i < end; ++i) {
            Particle& p = particles_[i];
            if (p.position.x < 0.0f) { p.position.x = 0.0f; p.velocity.x *= damp; }
            if (p.position.x > domW) { p.position.x = domW; p.velocity.x *= damp; }
            if (p.position.y < 0.0f) { p.position.y = 0.0f; p.velocity.y *= damp; }
            if (p.position.y > domH) { p.position.y = domH; p.velocity.y *= damp; }
        }
    };

    std::vector<std::thread> workers;
    for (int t = 0; t < actualThreads; ++t)
        workers.emplace_back(workerFunc, t);
    for (auto& w : workers)
        w.join();
}

// The per-stage overrides still use simple parallelFor (used if step() is called directly)
void CoarseGrainedSolver::computeDensityPressure() {
    const float h     = Config::SMOOTHING_RADIUS;
    const float mass  = Config::PARTICLE_MASS;
    const float rho0  = Config::REST_DENSITY;
    const float k     = Config::GAS_CONSTANT;
    const int   n     = static_cast<int>(particles_.size());

    parallelFor(n, [&](int start, int end) {
        std::vector<int> neighbours;
        for (int i = start; i < end; ++i) {
            Particle& pi = particles_[i];
            grid_.queryNeighbours(pi.position, h, particles_, neighbours);
            float density = 0.0f;
            for (int j : neighbours) {
                float r = (pi.position - particles_[j].position).length();
                density += mass * Kernels::poly6(r, h);
            }
            pi.density  = std::max(density, rho0 * 0.01f);
            pi.pressure = k * (pi.density - rho0);
        }
    });
}

void CoarseGrainedSolver::computeForces() {
    const float h    = Config::SMOOTHING_RADIUS;
    const float mass = Config::PARTICLE_MASS;
    const float mu   = Config::VISCOSITY;
    const int   n    = static_cast<int>(particles_.size());

    parallelFor(n, [&](int start, int end) {
        std::vector<int> neighbours;
        for (int i = start; i < end; ++i) {
            Particle& pi = particles_[i];
            Vec2 fP{0,0}, fV{0,0};
            grid_.queryNeighbours(pi.position, h, particles_, neighbours);
            for (int j : neighbours) {
                if (j == i) continue;
                const Particle& pj = particles_[j];
                Vec2  rij = pi.position - pj.position;
                float r   = rij.length();
                if (r < 1e-6f || r > h) continue;
                float pAvg = (pi.pressure + pj.pressure) / (2.0f * pj.density);
                fP += Kernels::spikyGrad(rij, r, h) * (-mass * pAvg);
                float lapW = Kernels::viscosityLaplacian(r, h);
                fV += (pj.velocity - pi.velocity) * (mu * mass * lapW / pj.density);
            }
            pi.force = fP + fV + Vec2{0.0f, Config::GRAVITY * pi.density};
        }
    });
}

void CoarseGrainedSolver::integrate(float dt) {
    const int n = static_cast<int>(particles_.size());
    parallelFor(n, [&](int start, int end) {
        for (int i = start; i < end; ++i) {
            Particle& p = particles_[i];
            Vec2 acc = p.force / p.density;
            p.velocity += acc * dt;
            p.position += p.velocity * dt;
        }
    });
}

void CoarseGrainedSolver::enforceBoundary() {
    const float damp = Config::BOUNDARY_DAMPING;
    const float w    = Config::DOMAIN_WIDTH;
    const float h    = Config::DOMAIN_HEIGHT;
    const int   n    = static_cast<int>(particles_.size());

    parallelFor(n, [&](int start, int end) {
        for (int i = start; i < end; ++i) {
            Particle& p = particles_[i];
            if (p.position.x < 0.0f) { p.position.x = 0.0f; p.velocity.x *= damp; }
            if (p.position.x > w)    { p.position.x = w;     p.velocity.x *= damp; }
            if (p.position.y < 0.0f) { p.position.y = 0.0f; p.velocity.y *= damp; }
            if (p.position.y > h)    { p.position.y = h;     p.velocity.y *= damp; }
        }
    });
}
