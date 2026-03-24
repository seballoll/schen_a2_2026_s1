#pragma once

#include "core/Particle.h"
#include "core/SpatialHash.h"
#include "Config.h"
#include "metrics/SimStats.h"
#include <vector>
#include <string>

// ============================================================
// SPHSolver — owns the particle array and executes the pipeline
//
// The simulation pipeline per timestep:
//   1. Build spatial hash (neighbour structure)
//   2. Compute density & pressure for every particle
//   3. Compute forces (pressure + viscosity + gravity)
//   4. Integrate (update velocity & position via Euler/Leapfrog)
//   5. Enforce boundary conditions
//
// Derived classes override the parallel execution strategy.
// ============================================================
class SPHSolver {
public:
    SPHSolver();
    virtual ~SPHSolver() = default;

    enum class PipelineStage : int {
        Neighbours      = 1,
        DensityPressure = 2,
        Forces          = 3,
        Integrate       = 4,
        Boundary        = 5,
    };

    /// Initialize particles in a "dam break" configuration.
    void initDamBreak(int numParticles, float domainW, float domainH);

    /// Run one full timestep (calls virtual step methods).
    void step(float dt);

    /// Run the pipeline up to the selected stage (inclusive).
    /// Useful for partial/dummy benchmarking of specific stages.
    void stepUpTo(float dt, PipelineStage stage);

    /// Access particles (for rendering / metrics).
    const std::vector<Particle>& particles() const { return particles_; }
    std::vector<Particle>&       particles()       { return particles_; }

    /// Human-readable model name.
    virtual std::string modelName() const { return "Base (Sequential)"; }

    // --- Simulated hardware counters (cycles/stalls) ---
    const SimStats& simStats() const { return simStats_; }
    void resetSimStats();
    double simulatedSeconds() const;

protected:
    std::vector<Particle> particles_;
    SpatialHash           grid_;

    // --- Simulated hardware model helpers ---
    void simAddWork(uint64_t cycles);
    void simAddIdle(uint64_t cycles);
    void simAddContextSwitch(uint64_t cycles);
    void simAddStall(StallKind kind, uint64_t latencyCycles, bool hidden);
    void simAddExposedStallCycles(uint64_t cycles);

    uint64_t simStepIndex_ = 0;
    uint64_t simSeed_      = Config::SIM_SEED;

    // --- Pipeline stages (overridden for parallelism) ---
    virtual void buildNeighbourStructure();
    virtual void computeDensityPressure();
    virtual void computeForces();
    virtual void integrate(float dt);
    virtual void enforceBoundary();

private:
    SimStats simStats_;
};
