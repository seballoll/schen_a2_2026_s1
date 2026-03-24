#include "threading/CoarseGrainedSolver.h"
#include "core/Kernels.h"
#include "Config.h"

#include "core/SimUtil.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

// ============================================================
// Coarse-Grained Multithreading (CGMT) — PROJECT MODEL
//
// Implemented as a cooperative scheduler in ONE OS thread.
// A "HW thread" runs until it hits a stall, then we switch.
// ============================================================

CoarseGrainedSolver::CoarseGrainedSolver(int numThreads)
    : numThreads_(numThreads) {}

std::string CoarseGrainedSolver::modelName() const {
    return "Coarse-Grained MT (" + std::to_string(numThreads_) + " threads)";
}

namespace {

struct HWThreadState {
    int start = 0;
    int end = 0;
    int next = 0;
    uint64_t blockedUntil = 0;
    std::vector<int> neighbours;
};

} // namespace

void CoarseGrainedSolver::runStageCGMT(
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
    }

    uint64_t now = simStats().totalCycles;
    int currentTid = 0;
    bool hasCurrent = false;

    auto anyRemaining = [&]() {
        for (const auto& t : threads) {
            if (t.next < t.end) return true;
        }
        return false;
    };

    auto isRunnable = [&](int tid) {
        const auto& t = threads[tid];
        return (t.next < t.end) && (now >= t.blockedUntil);
    };

    while (anyRemaining()) {
        int chosen = -1;

        if (hasCurrent && isRunnable(currentTid)) {
            chosen = currentTid;
        } else {
            int start = hasCurrent ? (currentTid + 1) % actualThreads : 0;
            for (int k = 0; k < actualThreads; ++k) {
                int tid = (start + k) % actualThreads;
                if (isRunnable(tid)) {
                    chosen = tid;
                    break;
                }
            }
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

        if (hasCurrent && chosen != currentTid) {
            simAddContextSwitch(Config::CGMT_CONTEXT_SWITCH_COST);
            now += Config::CGMT_CONTEXT_SWITCH_COST;
        }
        currentTid = chosen;
        hasCurrent = true;

        auto& t = threads[chosen];
        const int i = t.next;
        particleWork(chosen, i, t.neighbours);

        if (Config::ENABLE_CYCLE_MODEL) {
            uint64_t compute = cycleCost(chosen, i, t.neighbours);
            simAddWork(compute);
            now += compute;

            auto [kind, lat] = SimUtil::pickStall(simSeed_, simStepIndex_, stageId, i, chosen);
            if (lat > 0) {
                simAddStall(kind, lat, /*hidden=*/true);
                t.blockedUntil = now + lat;
                // On coarse-grained switching we immediately give up the core.
                hasCurrent = false;
            }
        }

        t.next++;
        if (t.next >= t.end) {
            hasCurrent = false;
        }
    }
}

void CoarseGrainedSolver::computeDensityPressure() {
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

    runStageCGMT(/*stageId=*/2, actualThreads, particleWork, cost);
}

void CoarseGrainedSolver::computeForces() {
    const float h    = Config::SMOOTHING_RADIUS;
    const float mass = Config::PARTICLE_MASS;
    const float mu   = Config::VISCOSITY;
    const int   n    = static_cast<int>(particles_.size());

    const int actualThreads = std::max(1, std::min(numThreads_, n));

    auto particleWork = [&](int /*tid*/, int i, std::vector<int>& neighbours) {
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
    };

    auto cost = [&](int /*tid*/, int /*i*/, const std::vector<int>& neighbours) -> uint64_t {
        uint64_t base = Config::CYCLES_NEIGH_QUERY_BASE +
                        Config::CYCLES_PER_NEIGHBOUR * static_cast<uint64_t>(neighbours.size());
        return base + (base / 2);
    };

    runStageCGMT(/*stageId=*/3, actualThreads, particleWork, cost);
}

void CoarseGrainedSolver::integrate(float dt) {
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

    runStageCGMT(/*stageId=*/4, actualThreads, particleWork, cost);
}

void CoarseGrainedSolver::enforceBoundary() {
    const float damp = Config::BOUNDARY_DAMPING;
    const float w    = Config::DOMAIN_WIDTH;
    const float h    = Config::DOMAIN_HEIGHT;
    const int   n    = static_cast<int>(particles_.size());

    const int actualThreads = std::max(1, std::min(numThreads_, n));

    auto particleWork = [&](int /*tid*/, int i, std::vector<int>& /*neighbours*/) {
        Particle& p = particles_[i];
        if (p.position.x < 0.0f) { p.position.x = 0.0f; p.velocity.x *= damp; }
        if (p.position.x > w)    { p.position.x = w;     p.velocity.x *= damp; }
        if (p.position.y < 0.0f) { p.position.y = 0.0f; p.velocity.y *= damp; }
        if (p.position.y > h)    { p.position.y = h;     p.velocity.y *= damp; }
    };

    auto cost = [&](int /*tid*/, int /*i*/, const std::vector<int>& /*neighbours*/) -> uint64_t {
        return Config::CYCLES_BOUNDARY_PER_P;
    };

    runStageCGMT(/*stageId=*/5, actualThreads, particleWork, cost);
}
