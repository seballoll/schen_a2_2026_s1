#include "threading/FineGrainedSolver.h"
#include "core/Kernels.h"
#include "Config.h"
#include <functional>
#include <vector>

// ============================================================
// Fine-Grained Multithreading (FGMT)
//
// Strategy: for EACH pipeline stage, spawn `numThreads_` threads
// that each process a slice of the particle array.  After every
// stage all threads join (implicit barrier), then the next stage
// starts.  This frequent synchronisation models the cycle-by-
// cycle interleaving of hardware fine-grained MT.
// ============================================================

FineGrainedSolver::FineGrainedSolver(int numThreads)
    : numThreads_(numThreads) {}

std::string FineGrainedSolver::modelName() const {
    return "Fine-Grained MT (" + std::to_string(numThreads_) + " threads)";
}

// --- parallelFor helper ---
void FineGrainedSolver::parallelFor(int n, std::function<void(int, int)> func) {
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
// Stage 2: Density & Pressure — parallel over particles
// ============================================================
void FineGrainedSolver::computeDensityPressure() {
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
    // ── implicit barrier (all threads joined) ──
}

// ============================================================
// Stage 3: Forces — parallel over particles
// ============================================================
void FineGrainedSolver::computeForces() {
    const float h    = Config::SMOOTHING_RADIUS;
    const float mass = Config::PARTICLE_MASS;
    const float mu   = Config::VISCOSITY;
    const int   n    = static_cast<int>(particles_.size());

    parallelFor(n, [&](int start, int end) {
        std::vector<int> neighbours;
        for (int i = start; i < end; ++i) {
            Particle& pi = particles_[i];
            Vec2 fPressure{0, 0}, fViscosity{0, 0};

            grid_.queryNeighbours(pi.position, h, particles_, neighbours);
            for (int j : neighbours) {
                if (j == i) continue;
                const Particle& pj = particles_[j];
                Vec2  rij = pi.position - pj.position;
                float r   = rij.length();
                if (r < 1e-6f || r > h) continue;

                float pressureAvg = (pi.pressure + pj.pressure) / (2.0f * pj.density);
                fPressure  += Kernels::spikyGrad(rij, r, h) * (-mass * pressureAvg);

                float lapW  = Kernels::viscosityLaplacian(r, h);
                fViscosity += (pj.velocity - pi.velocity) * (mu * mass * lapW / pj.density);
            }
            Vec2 fGravity{0.0f, Config::GRAVITY * pi.density};
            pi.force = fPressure + fViscosity + fGravity;
        }
    });
}

// ============================================================
// Stage 4: Integration — parallel over particles
// ============================================================
void FineGrainedSolver::integrate(float dt) {
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

// ============================================================
// Stage 5: Boundary — parallel over particles
// ============================================================
void FineGrainedSolver::enforceBoundary() {
    const float damp = Config::BOUNDARY_DAMPING;
    const float w    = Config::DOMAIN_WIDTH;
    const float h    = Config::DOMAIN_HEIGHT;
    const int   n    = static_cast<int>(particles_.size());

    parallelFor(n, [&](int start, int end) {
        for (int i = start; i < end; ++i) {
            Particle& p = particles_[i];
            if (p.position.x < 0.0f)  { p.position.x = 0.0f;  p.velocity.x *= damp; }
            if (p.position.x > w)     { p.position.x = w;      p.velocity.x *= damp; }
            if (p.position.y < 0.0f)  { p.position.y = 0.0f;  p.velocity.y *= damp; }
            if (p.position.y > h)     { p.position.y = h;      p.velocity.y *= damp; }
        }
    });
}
