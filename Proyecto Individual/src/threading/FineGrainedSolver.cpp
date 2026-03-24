#include "threading/FineGrainedSolver.h"
#include "core/Kernels.h"
#include "Config.h"

#include "core/SimUtil.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

// ============================================================
// Fine-Grained Multithreading (FGMT) — PROJECT MODEL
//
// Implemented as a cooperative scheduler in ONE OS thread.
// - "Threads" are simulated hardware contexts.
// - Switch every small quantum to hide stalls.
// ============================================================

FineGrainedSolver::FineGrainedSolver(int numThreads)
    : numThreads_(numThreads) {}

std::string FineGrainedSolver::modelName() const {
    return "Fine-Grained MT (" + std::to_string(numThreads_) + " threads)";
}

namespace {

struct HWThreadState {
    int start = 0;
    int end = 0;
    int next = 0;
    uint64_t blockedUntil = 0;
    uint64_t quantumLeft = 0;
    std::vector<int> neighbours;
};

} // namespace

void FineGrainedSolver::runStageFGMT(
    int stageId,
    int actualThreads,
    const std::function<void(int, int, std::vector<int>&)>& particleWork,
    const std::function<uint64_t(int, int, const std::vector<int>&)>& cycleCost)
{
    const int n = static_cast<int>(particles_.size());
    if (n <= 0 || actualThreads <= 0) return;

    const int chunkSize = (n + actualThreads - 1) / actualThreads;
    std::vector<HWThreadState> threads(static_cast<size_t>(actualThreads));
    for (int tid = 0; tid < actualThreads; ++tid) {
        int start = tid * chunkSize;
        int end = std::min(start + chunkSize, n);
        threads[tid].start = start;
        threads[tid].end = end;
        threads[tid].next = start;
        threads[tid].blockedUntil = 0;
        threads[tid].quantumLeft = Config::FGMT_QUANTUM_CYCLES;
    }

    uint64_t now = simStats().totalCycles;
    int rrStart = 0;

    auto anyRemaining = [&]() {
        for (const auto& t : threads) {
            if (t.next < t.end) return true;
        }
        return false;
    };

    while (anyRemaining()) {
        int chosen = -1;
        for (int k = 0; k < actualThreads; ++k) {
            int tid = (rrStart + k) % actualThreads;
            auto& t = threads[tid];
            if (t.next >= t.end) continue;
            if (now < t.blockedUntil) continue;
            chosen = tid;
            break;
        }

        if (chosen < 0) {
            uint64_t nextWake = std::numeric_limits<uint64_t>::max();
            for (const auto& t : threads) {
                if (t.next >= t.end) continue;
                nextWake = std::min(nextWake, t.blockedUntil);
            }
            if (nextWake == std::numeric_limits<uint64_t>::max() || nextWake <= now) break;
            uint64_t idle = nextWake - now;
            simAddIdle(idle);
            simAddExposedStallCycles(idle);
            now = nextWake;
            continue;
        }

        auto& t = threads[chosen];
        const int i = t.next;
        particleWork(chosen, i, t.neighbours);

        if (Config::ENABLE_CYCLE_MODEL) {
            uint64_t compute = cycleCost(chosen, i, t.neighbours);
            simAddWork(compute);
            now += compute;

            auto [kind, lat] = SimUtil::pickStall(simSeed_, simStepIndex_, stageId, i, chosen);
            if (lat > 0) {
                // Hidden by switching to other HW threads.
                simAddStall(kind, lat, /*hidden=*/true);
                t.blockedUntil = now + lat;
            }

            if (compute >= t.quantumLeft) t.quantumLeft = 0;
            else t.quantumLeft -= compute;

            if (t.quantumLeft == 0) {
                simAddContextSwitch(Config::FGMT_CONTEXT_SWITCH_COST);
                now += Config::FGMT_CONTEXT_SWITCH_COST;
                t.quantumLeft = Config::FGMT_QUANTUM_CYCLES;
                rrStart = (chosen + 1) % actualThreads;
            } else {
                rrStart = chosen;
            }
        } else {
            rrStart = (chosen + 1) % actualThreads;
        }

        t.next++;
    }
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

    const int actualThreads = std::max(1, std::min(numThreads_, n));

    auto particleWork = [&](int /*tid*/, int i, std::vector<int>& neighbours) {
        Particle& pi = particles_[i];
        grid_.queryNeighbours(pi.position, h, particles_, neighbours);

        float density = 0.0f;
        for (int j : neighbours) {
            float r = (pi.position - particles_[j].position).length();
            density += mass * Kernels::poly6(r, h);
        }
        pi.density  = std::max(density, rho0 * 0.01f);
        pi.pressure = k * (pi.density - rho0);
    };

    auto cost = [&](int /*tid*/, int /*i*/, const std::vector<int>& neighbours) -> uint64_t {
        return Config::CYCLES_NEIGH_QUERY_BASE +
               Config::CYCLES_PER_NEIGHBOUR * static_cast<uint64_t>(neighbours.size());
    };

    runStageFGMT(/*stageId=*/2, actualThreads, particleWork, cost);
}

// ============================================================
// Stage 3: Forces — parallel over particles
// ============================================================
void FineGrainedSolver::computeForces() {
    const float h    = Config::SMOOTHING_RADIUS;
    const float mass = Config::PARTICLE_MASS;
    const float mu   = Config::VISCOSITY;
    const int   n    = static_cast<int>(particles_.size());

    const int actualThreads = std::max(1, std::min(numThreads_, n));

    auto particleWork = [&](int /*tid*/, int i, std::vector<int>& neighbours) {
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
    };

    auto cost = [&](int /*tid*/, int /*i*/, const std::vector<int>& neighbours) -> uint64_t {
        uint64_t base = Config::CYCLES_NEIGH_QUERY_BASE +
                        Config::CYCLES_PER_NEIGHBOUR * static_cast<uint64_t>(neighbours.size());
        return base + (base / 2);
    };

    runStageFGMT(/*stageId=*/3, actualThreads, particleWork, cost);
}

// ============================================================
// Stage 4: Integration — parallel over particles
// ============================================================
void FineGrainedSolver::integrate(float dt) {
    const int n = static_cast<int>(particles_.size());
    const int actualThreads = std::max(1, std::min(numThreads_, n));

    auto particleWork = [&](int /*tid*/, int i, std::vector<int>& /*neighbours*/) {
        Particle& p = particles_[i];
        Vec2 acc = p.force / p.density;
        p.velocity += acc * dt;
        p.position += p.velocity * dt;
    };

    auto cost = [&](int /*tid*/, int /*i*/, const std::vector<int>& /*neighbours*/) -> uint64_t {
        return Config::CYCLES_INTEGRATE_PER_P;
    };

    runStageFGMT(/*stageId=*/4, actualThreads, particleWork, cost);
}

// ============================================================
// Stage 5: Boundary — parallel over particles
// ============================================================
void FineGrainedSolver::enforceBoundary() {
    const float damp = Config::BOUNDARY_DAMPING;
    const float w    = Config::DOMAIN_WIDTH;
    const float h    = Config::DOMAIN_HEIGHT;
    const int   n    = static_cast<int>(particles_.size());

    const int actualThreads = std::max(1, std::min(numThreads_, n));

    auto particleWork = [&](int /*tid*/, int i, std::vector<int>& /*neighbours*/) {
        Particle& p = particles_[i];
        if (p.position.x < 0.0f)  { p.position.x = 0.0f;  p.velocity.x *= damp; }
        if (p.position.x > w)     { p.position.x = w;      p.velocity.x *= damp; }
        if (p.position.y < 0.0f)  { p.position.y = 0.0f;  p.velocity.y *= damp; }
        if (p.position.y > h)     { p.position.y = h;      p.velocity.y *= damp; }
    };

    auto cost = [&](int /*tid*/, int /*i*/, const std::vector<int>& /*neighbours*/) -> uint64_t {
        return Config::CYCLES_BOUNDARY_PER_P;
    };

    runStageFGMT(/*stageId=*/5, actualThreads, particleWork, cost);
}
