#include "threading/CMPSolver.h"
#include "core/Kernels.h"
#include "Config.h"
#include <functional>
#include <vector>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#include <fstream>
#include <string>
#include <set>
#endif

// ============================================================
// CMP (Chip Multiprocessing) Solver
//
// Strategy: pin each thread to a SEPARATE physical core,
// ensuring truly independent parallel execution with no
// resource sharing.
//
// On Linux we read /sys/devices/system/cpu/cpuN/topology/
// core_id to map logical → physical cores and select one
// logical core per physical core.
// ============================================================

CMPSolver::CMPSolver(int numThreads)
    : numThreads_(numThreads) {}

std::string CMPSolver::modelName() const {
    return "CMP / Chip Multiprocessing (" + std::to_string(numThreads_) + " threads)";
}

void CMPSolver::pinToPhysicalCore(int tid) {
#ifdef __linux__
    // Build a list of one logical CPU per physical core
    static std::vector<int> physicalCoreMap = []() {
        std::vector<int> coreMap;
        std::set<int> seenCores;
        unsigned int nproc = std::thread::hardware_concurrency();
        for (unsigned int cpu = 0; cpu < nproc; ++cpu) {
            std::string path = "/sys/devices/system/cpu/cpu" +
                               std::to_string(cpu) +
                               "/topology/core_id";
            std::ifstream f(path);
            int coreId = -1;
            if (f >> coreId) {
                if (seenCores.find(coreId) == seenCores.end()) {
                    seenCores.insert(coreId);
                    coreMap.push_back(static_cast<int>(cpu));
                }
            }
        }
        // Fallback if topology not readable
        if (coreMap.empty()) {
            for (unsigned int i = 0; i < nproc; ++i)
                coreMap.push_back(static_cast<int>(i));
        }
        return coreMap;
    }();

    int logicalCpu = physicalCoreMap[tid % physicalCoreMap.size()];
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(logicalCpu, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
    (void)tid;
}

void CMPSolver::parallelForCMP(int n, std::function<void(int, int)> func) {
    std::vector<std::thread> threads;
    int chunkSize = (n + numThreads_ - 1) / numThreads_;

    for (int t = 0; t < numThreads_; ++t) {
        int start = t * chunkSize;
        int end   = std::min(start + chunkSize, n);
        if (start >= n) break;

        threads.emplace_back([=, &func]() {
            pinToPhysicalCore(t);
            func(start, end);
        });
    }
    for (auto& th : threads) th.join();
}

// --- Pipeline stages ---

void CMPSolver::computeDensityPressure() {
    const float h     = Config::SMOOTHING_RADIUS;
    const float mass  = Config::PARTICLE_MASS;
    const float rho0  = Config::REST_DENSITY;
    const float k     = Config::GAS_CONSTANT;
    const int   n     = static_cast<int>(particles_.size());

    parallelForCMP(n, [&](int start, int end) {
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

void CMPSolver::computeForces() {
    const float h    = Config::SMOOTHING_RADIUS;
    const float mass = Config::PARTICLE_MASS;
    const float mu   = Config::VISCOSITY;
    const int   n    = static_cast<int>(particles_.size());

    parallelForCMP(n, [&](int start, int end) {
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

void CMPSolver::integrate(float dt) {
    const int n = static_cast<int>(particles_.size());
    parallelForCMP(n, [&](int start, int end) {
        for (int i = start; i < end; ++i) {
            Particle& p = particles_[i];
            Vec2 acc = p.force / p.density;
            p.velocity += acc * dt;
            p.position += p.velocity * dt;
        }
    });
}

void CMPSolver::enforceBoundary() {
    const float damp = Config::BOUNDARY_DAMPING;
    const float w    = Config::DOMAIN_WIDTH;
    const float hDom = Config::DOMAIN_HEIGHT;
    const int   n    = static_cast<int>(particles_.size());

    parallelForCMP(n, [&](int start, int end) {
        for (int i = start; i < end; ++i) {
            Particle& p = particles_[i];
            if (p.position.x < 0.0f)  { p.position.x = 0.0f;   p.velocity.x *= damp; }
            if (p.position.x > w)     { p.position.x = w;       p.velocity.x *= damp; }
            if (p.position.y < 0.0f)  { p.position.y = 0.0f;   p.velocity.y *= damp; }
            if (p.position.y > hDom)  { p.position.y = hDom;   p.velocity.y *= damp; }
        }
    });
}
