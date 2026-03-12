#include "core/SPHSolver.h"
#include "core/Kernels.h"
#include "Config.h"
#include <cmath>

// ============================================================
// Constructor
// ============================================================
SPHSolver::SPHSolver()
    : grid_(Config::CELL_SIZE, Config::HASH_TABLE_SIZE) {}

// ============================================================
// initDamBreak — place particles in a rectangular block on the
// left side of the domain (classic "dam break" scenario).
// ============================================================
void SPHSolver::initDamBreak(int numParticles, float domainW, float domainH) {
    particles_.clear();
    particles_.reserve(numParticles);

    // Arrange particles in a grid filling ~30% width, ~80% height
    float blockW = domainW * 0.30f;
    float blockH = domainH * 0.80f;
    float startX = domainW * 0.05f;
    float startY = domainH * 0.10f;

    // Determine spacing
    float spacing = std::sqrt((blockW * blockH) / static_cast<float>(numParticles));
    int cols = static_cast<int>(blockW / spacing);
    int rows = static_cast<int>(blockH / spacing);
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;

    int placed = 0;
    for (int r = 0; r < rows && placed < numParticles; ++r) {
        for (int c = 0; c < cols && placed < numParticles; ++c) {
            float x = startX + c * spacing + spacing * 0.5f;
            float y = startY + r * spacing + spacing * 0.5f;
            particles_.emplace_back(Vec2{x, y});
            ++placed;
        }
    }
}

// ============================================================
// step — execute the full SPH pipeline for one timestep
// ============================================================
void SPHSolver::step(float dt) {
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
// Pipeline stages — default SEQUENTIAL implementations
// ============================================================

void SPHSolver::buildNeighbourStructure() {
    grid_.build(particles_);
}

void SPHSolver::computeDensityPressure() {
    const float h     = Config::SMOOTHING_RADIUS;
    const float mass  = Config::PARTICLE_MASS;
    const float rho0  = Config::REST_DENSITY;
    const float k     = Config::GAS_CONSTANT;

    std::vector<int> neighbours;

    for (auto& pi : particles_) {
        grid_.queryNeighbours(pi.position, h, particles_, neighbours);

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
