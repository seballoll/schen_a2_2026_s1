#include "threading/SMTSolver.h"
#include "core/Kernels.h"
#include "Config.h"
#include <functional>
#include <vector>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

// ============================================================
// SMT / Hyper-Threading Solver
//
// Strategy: identical parallel-for approach, but we PIN threads
// to LOGICAL cores that share the same PHYSICAL core.
//
// On a system with N physical cores and HT enabled, logical
// cores 0..N-1 are first HW thread, N..2N-1 are the second.
// (This is the typical Linux topology; adjust if needed.)
//
// This lets us observe if sharing execution units (ALUs,
// caches) provides extra throughput (ILP extraction).
// ============================================================

SMTSolver::SMTSolver(int numThreads)
    : numThreads_(numThreads) {}

std::string SMTSolver::modelName() const {
    return "SMT / HyperThreading (" + std::to_string(numThreads_) + " threads)";
}

void SMTSolver::pinToLogicalCore(int tid) {
#ifdef __linux__
    // Attempt to pin to the logical core `tid`.
    // On HT systems, sibling cores share a physical core.
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(tid % std::thread::hardware_concurrency(), &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
    (void)tid; // suppress unused warning on non-Linux
}

void SMTSolver::parallelForSMT(int n, std::function<void(int, int)> func) {
    std::vector<std::thread> threads;
    int chunkSize = (n + numThreads_ - 1) / numThreads_;

    for (int t = 0; t < numThreads_; ++t) {
        int start = t * chunkSize;
        int end   = std::min(start + chunkSize, n);
        if (start >= n) break;

        threads.emplace_back([=, &func]() {
            pinToLogicalCore(t);
            func(start, end);
        });
    }
    for (auto& th : threads) th.join();
}

// --- Pipeline stages ---

void SMTSolver::computeDensityPressure() {
    const float h     = Config::SMOOTHING_RADIUS;
    const float mass  = Config::PARTICLE_MASS;
    const float rho0  = Config::REST_DENSITY;
    const float k     = Config::GAS_CONSTANT;
    const int   n     = static_cast<int>(particles_.size());

    parallelForSMT(n, [&](int start, int end) {
        std::vector<int> neighbours;
        for (int i = start; i < end; ++i) {
            Particle& pi = particles_[i];
            grid_.queryNeighbours(pi.position, h, particles_, neighbours);
            float density = 0.0f;
            for (int j : neighbours)
                density += mass * Kernels::poly6(
                    (pi.position - particles_[j].position).length(), h);
            pi.density  = std::max(density, rho0 * 0.01f);
            pi.pressure = k * (pi.density - rho0);
        }
    });
}

void SMTSolver::computeForces() {
    const float h    = Config::SMOOTHING_RADIUS;
    const float mass = Config::PARTICLE_MASS;
    const float mu   = Config::VISCOSITY;
    const int   n    = static_cast<int>(particles_.size());

    parallelForSMT(n, [&](int start, int end) {
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
                fV += (pj.velocity - pi.velocity) *
                      (mu * mass * Kernels::viscosityLaplacian(r, h) / pj.density);
            }
            pi.force = fP + fV + Vec2{0.0f, Config::GRAVITY * pi.density};
        }
    });
}

void SMTSolver::integrate(float dt) {
    const int n = static_cast<int>(particles_.size());
    parallelForSMT(n, [&](int start, int end) {
        for (int i = start; i < end; ++i) {
            Particle& p = particles_[i];
            Vec2 acc = p.force / p.density;
            p.velocity += acc * dt;
            p.position += p.velocity * dt;
        }
    });
}

void SMTSolver::enforceBoundary() {
    const float damp = Config::BOUNDARY_DAMPING;
    const float w    = Config::DOMAIN_WIDTH;
    const float hDom = Config::DOMAIN_HEIGHT;
    const int   n    = static_cast<int>(particles_.size());

    parallelForSMT(n, [&](int start, int end) {
        for (int i = start; i < end; ++i) {
            Particle& p = particles_[i];
            if (p.position.x < 0.0f)   { p.position.x = 0.0f;   p.velocity.x *= damp; }
            if (p.position.x > w)      { p.position.x = w;       p.velocity.x *= damp; }
            if (p.position.y < 0.0f)   { p.position.y = 0.0f;   p.velocity.y *= damp; }
            if (p.position.y > hDom)   { p.position.y = hDom;   p.velocity.y *= damp; }
        }
    });
}
