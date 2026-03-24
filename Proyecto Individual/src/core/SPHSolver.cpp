#include "core/SPHSolver.h"
#include "core/Kernels.h"
#include "Config.h"
#include "core/SimUtil.h"
#include <cmath>

// ============================================================
// Constructor
// ============================================================
SPHSolver::SPHSolver()
    : grid_(Config::CELL_SIZE, Config::HASH_TABLE_SIZE) {}

void SPHSolver::resetSimStats() {
    simStats_.reset();
    simStepIndex_ = 0;
}

double SPHSolver::simulatedSeconds() const {
    if (Config::CPU_CLOCK_HZ <= 0.0) return 0.0;
    return static_cast<double>(simStats_.totalCycles) / Config::CPU_CLOCK_HZ;
}

void SPHSolver::simAddWork(uint64_t cycles) {
    if (!Config::ENABLE_CYCLE_MODEL || cycles == 0) return;
    simStats_.totalCycles += cycles;
    simStats_.workCycles += cycles;
}

void SPHSolver::simAddIdle(uint64_t cycles) {
    if (!Config::ENABLE_CYCLE_MODEL || cycles == 0) return;
    simStats_.totalCycles += cycles;
    simStats_.idleCycles += cycles;
}

void SPHSolver::simAddContextSwitch(uint64_t cycles) {
    if (!Config::ENABLE_CYCLE_MODEL || cycles == 0) return;
    simStats_.totalCycles += cycles;
    simStats_.contextSwitchCycles += cycles;
}

void SPHSolver::simAddStall(StallKind kind, uint64_t latencyCycles, bool hidden) {
    if (!Config::ENABLE_CYCLE_MODEL || latencyCycles == 0) return;

    simStats_.stallEvents++;
    simStats_.stallCyclesTotal += latencyCycles;
    if (hidden) simStats_.stallCyclesHidden += latencyCycles;
    else simStats_.stallCyclesExposed += latencyCycles;

    switch (kind) {
        case StallKind::L1:     simStats_.stallEventsL1++; break;
        case StallKind::L2:     simStats_.stallEventsL2++; break;
        case StallKind::L3:     simStats_.stallEventsL3++; break;
        case StallKind::Memory: simStats_.stallEventsMem++; break;
        case StallKind::Sync:   simStats_.stallEventsSync++; break;
    }
}

void SPHSolver::simAddExposedStallCycles(uint64_t cycles) {
    if (!Config::ENABLE_CYCLE_MODEL || cycles == 0) return;
    simStats_.stallCyclesTotal += cycles;
    simStats_.stallCyclesExposed += cycles;
}

// ============================================================
// initDamBreak — place particles in a rectangular block on the
// left side of the domain (classic "dam break" scenario).
//
// Uses PARTICLE_SPACING for the grid to ensure the density
// estimate matches REST_DENSITY when the fluid is at rest.
// ============================================================
void SPHSolver::initDamBreak(int numParticles, float domainW, float domainH) {
    resetSimStats();
    particles_.clear();
    particles_.reserve(numParticles);

    // Use the configured spacing so density is well-defined
    float spacing = Config::PARTICLE_SPACING;

    // Place in a tall column on the left ~25% of the domain
    // starting a small offset from the wall
    float startX = spacing;
    float startY = spacing;

    // How many columns fit in 25% of the domain?
    int cols = static_cast<int>((domainW * 0.25f) / spacing);
    if (cols < 1) cols = 1;

    // How many rows do we need?
    int rows = (numParticles + cols - 1) / cols;

    int placed = 0;
    for (int r = 0; r < rows && placed < numParticles; ++r) {
        for (int c = 0; c < cols && placed < numParticles; ++c) {
            float x = startX + c * spacing;
            float y = startY + r * spacing;

            // Small jitter to break perfect lattice symmetry
            float jx = ((placed * 7 + 3) % 11 - 5) * 0.01f * spacing;
            float jy = ((placed * 13 + 7) % 11 - 5) * 0.01f * spacing;

            particles_.emplace_back(Vec2{x + jx, y + jy});
            ++placed;
        }
    }
}

// ============================================================
// step — execute the full SPH pipeline for one timestep
// ============================================================
void SPHSolver::step(float dt) {
    ++simStepIndex_;

    // 1. Rebuild neighbour structure
    buildNeighbourStructure();

    // 2. Compute density & pressure
    computeDensityPressure();

    // ── BARRIER (implicit: sequential runs in order) ──

    // 3. Compute forces
    computeForces();

    // 4. Integrate (Euler step)
    integrate(dt);

    // 5. Boundary conditions
    enforceBoundary();
}

// ============================================================
// stepUpTo — execute the SPH pipeline up to a given stage
// ============================================================
void SPHSolver::stepUpTo(float dt, PipelineStage stage) {
    ++simStepIndex_;

    const int s = static_cast<int>(stage);
    if (s >= static_cast<int>(PipelineStage::Neighbours)) {
        buildNeighbourStructure();
    }
    if (s >= static_cast<int>(PipelineStage::DensityPressure)) {
        computeDensityPressure();
    }
    if (s >= static_cast<int>(PipelineStage::Forces)) {
        computeForces();
    }
    if (s >= static_cast<int>(PipelineStage::Integrate)) {
        integrate(dt);
    }
    if (s >= static_cast<int>(PipelineStage::Boundary)) {
        enforceBoundary();
    }
}

// ============================================================
// Pipeline stages — default SEQUENTIAL implementations
// ============================================================

void SPHSolver::buildNeighbourStructure() {
    grid_.build(particles_);

    if (Config::ENABLE_CYCLE_MODEL) {
        simAddWork(Config::CYCLES_BUILD_HASH_PER_P * static_cast<uint64_t>(particles_.size()));
    }
}

void SPHSolver::computeDensityPressure() {
    const float h     = Config::SMOOTHING_RADIUS;
    const float mass  = Config::PARTICLE_MASS;
    const float rho0  = Config::REST_DENSITY;
    const float k     = Config::GAS_CONSTANT;

    std::vector<int> neighbours;

    for (int i = 0; i < static_cast<int>(particles_.size()); ++i) {
        auto& pi = particles_[i];
        grid_.queryNeighbours(pi.position, h, particles_, neighbours);

        if (Config::ENABLE_CYCLE_MODEL) {
            uint64_t compute = Config::CYCLES_NEIGH_QUERY_BASE +
                               Config::CYCLES_PER_NEIGHBOUR * static_cast<uint64_t>(neighbours.size());
            simAddWork(compute);

            auto [kind, lat] = SimUtil::pickStall(simSeed_, simStepIndex_, 2, i);
            if (lat > 0) {
                // In the sequential baseline stalls are fully exposed.
                simAddStall(kind, lat, /*hidden=*/false);
                simAddIdle(lat);
            }
        }

        float density = 0.0f;
        for (int j : neighbours) {
            float r = (pi.position - particles_[j].position).length();
            density += mass * Kernels::poly6(r, h);
        }
        pi.density  = std::max(density, rho0 * 0.01f); // avoid zero
        pi.pressure = k * (pi.density - rho0);           // Tait EOS (linearised)
    }
}

void SPHSolver::computeForces() {
    const float h    = Config::SMOOTHING_RADIUS;
    const float mass = Config::PARTICLE_MASS;
    const float mu   = Config::VISCOSITY;

    std::vector<int> neighbours;

    for (int i = 0; i < static_cast<int>(particles_.size()); ++i) {
        Particle& pi = particles_[i];
        Vec2 fPressure{0, 0};
        Vec2 fViscosity{0, 0};

        grid_.queryNeighbours(pi.position, h, particles_, neighbours);

        if (Config::ENABLE_CYCLE_MODEL) {
            uint64_t base = Config::CYCLES_NEIGH_QUERY_BASE +
                            Config::CYCLES_PER_NEIGHBOUR * static_cast<uint64_t>(neighbours.size());
            // Forces is heavier than density, so scale the compute cost.
            simAddWork(base + (base / 2));

            auto [kind, lat] = SimUtil::pickStall(simSeed_, simStepIndex_, 3, i);
            if (lat > 0) {
                simAddStall(kind, lat, /*hidden=*/false);
                simAddIdle(lat);
            }
        }

        for (int j : neighbours) {
            if (j == i) continue;
            const Particle& pj = particles_[j];

            Vec2  rij = pi.position - pj.position;
            float r   = rij.length();

            if (r < 1e-6f || r > h) continue;

            // Pressure force
            float pressureAvg = (pi.pressure + pj.pressure) / (2.0f * pj.density);
            Vec2  gradW = Kernels::spikyGrad(rij, r, h);
            fPressure += gradW * (-mass * pressureAvg);

            // Viscosity force
            float lapW = Kernels::viscosityLaplacian(r, h);
            fViscosity += (pj.velocity - pi.velocity) * (mu * mass * lapW / pj.density);
        }

        // Gravity
        Vec2 fGravity{0.0f, Config::GRAVITY * pi.density};

        pi.force = fPressure + fViscosity + fGravity;
    }
}

void SPHSolver::integrate(float dt) {
    if (Config::ENABLE_CYCLE_MODEL) {
        simAddWork(Config::CYCLES_INTEGRATE_PER_P * static_cast<uint64_t>(particles_.size()));
    }
    for (auto& p : particles_) {
        // a = F / ρ
        Vec2 acceleration = p.force / p.density;
        p.velocity += acceleration * dt;
        p.position += p.velocity   * dt;
    }
}

void SPHSolver::enforceBoundary() {
    const float damp = Config::BOUNDARY_DAMPING;
    const float w    = Config::DOMAIN_WIDTH;
    const float h    = Config::DOMAIN_HEIGHT;

    if (Config::ENABLE_CYCLE_MODEL) {
        simAddWork(Config::CYCLES_BOUNDARY_PER_P * static_cast<uint64_t>(particles_.size()));
    }
    for (auto& p : particles_) {
        // Left wall
        if (p.position.x < 0.0f) {
            p.position.x = 0.0f;
            p.velocity.x *= damp;
        }
        // Right wall
        if (p.position.x > w) {
            p.position.x = w;
            p.velocity.x *= damp;
        }
        // Bottom wall
        if (p.position.y < 0.0f) {
            p.position.y = 0.0f;
            p.velocity.y *= damp;
        }
        // Top wall
        if (p.position.y > h) {
            p.position.y = h;
            p.velocity.y *= damp;
        }
    }
}
