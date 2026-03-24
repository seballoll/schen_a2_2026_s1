#pragma once

#include <cmath>

// ============================================================
// Global simulation parameters — tweak these to experiment
//
// SPH parameter tuning notes:
//   • Particle spacing ≈ h / 2  gives ~20 neighbours in 2D
//   • Mass = REST_DENSITY * spacing²  (2D volume = spacing²)
//   • GAS_CONSTANT controls stiffness: higher = more rigid fluid
//   • VISCOSITY smooths velocities: higher = thicker fluid
//   • DT must satisfy CFL: dt < 0.4 * h / max_velocity
// ============================================================

namespace Config {

    // --- Window / Rendering ---
    constexpr int    WINDOW_WIDTH        = 1200;
    constexpr int    WINDOW_HEIGHT       = 800;
    constexpr float  PARTICLE_RENDER_RADIUS = 2.0f;

    // --- Domain ---
    constexpr float  DOMAIN_WIDTH        = 40.0f;
    constexpr float  DOMAIN_HEIGHT       = 30.0f;

    // --- SPH Parameters ---
    constexpr int    NUM_PARTICLES       = 2000;
    constexpr float  SMOOTHING_RADIUS    = 0.8f;       // h — kernel support radius
    constexpr float  REST_DENSITY        = 1000.0f;     // kg/m² (2D rest density)
    constexpr float  PARTICLE_SPACING    = 0.4f;        // ≈ h/2 — initial grid spacing
    // Mass derived so that SPH density ≈ REST_DENSITY at rest:
    //   mass = REST_DENSITY * spacing²  (2D particle volume)
    constexpr float  PARTICLE_MASS       = REST_DENSITY * PARTICLE_SPACING * PARTICLE_SPACING;
    constexpr float  GAS_CONSTANT        = 500.0f;      // stiffness (Tait EOS)
    constexpr float  VISCOSITY           = 100.0f;       // dynamic viscosity μ
    constexpr float  GRAVITY             = -9.81f;       // m/s²

    // --- Time Integration ---
    constexpr float  DT                  = 0.010f;      // timestep (seconds)
    constexpr int    MAX_STEPS           = 50000;        // safety cap

    // --- Spatial Hash ---
    constexpr float  CELL_SIZE           = SMOOTHING_RADIUS;
    constexpr int    HASH_TABLE_SIZE     = 10007;

    // --- Threading defaults ---
    constexpr int    DEFAULT_THREAD_COUNT = 4;

    // --- Boundary ---
    constexpr float  BOUNDARY_DAMPING    = -0.5f;

    // ============================================================
    // Hardware / scheduling model (for the project "laboratory")
    //
    // These parameters drive the *simulated* counters reported as
    // cycles/stalls. They do NOT affect the real wall-clock timer.
    // ============================================================

    // If true, solvers may accumulate simulated cycles/stalls.
    constexpr bool   ENABLE_CYCLE_MODEL  = true;

    // Nominal CPU clock to convert cycles -> seconds.
    // Keep this as a configuration knob (not auto-detected).
    constexpr double CPU_CLOCK_HZ        = 3.0e9; // 3 GHz

    // Deterministic seed for stall generation.
    constexpr uint64_t SIM_SEED          = 0xC0FFEEULL;

    // --- Fine-grained multithreading (FGMT) ---
    // Quantum in simulated cycles before a forced context switch.
    constexpr uint64_t FGMT_QUANTUM_CYCLES      = 50;
    constexpr uint64_t FGMT_CONTEXT_SWITCH_COST = 20;

    // --- Coarse-grained multithreading (CGMT) ---
    // In CGMT we run until a long stall (or stage end).
    constexpr uint64_t CGMT_CONTEXT_SWITCH_COST = 20;

    // --- Stall latencies (cycles) ---
    constexpr uint64_t STALL_L1_CYCLES   = 4;
    constexpr uint64_t STALL_L2_CYCLES   = 12;
    constexpr uint64_t STALL_L3_CYCLES   = 40;
    constexpr uint64_t STALL_MEM_CYCLES  = 200;

    // --- Stall probabilities (per particle neighbour-query) ---
    // Tunable: these should be adjusted using perf/VTune observations.
    constexpr double  PROB_L2_MISS       = 0.020;
    constexpr double  PROB_L3_MISS       = 0.010;
    constexpr double  PROB_MEM_STALL     = 0.003;

    // --- Base compute costs (cycles) ---
    // Simple linear model based on neighbour count.
    constexpr uint64_t CYCLES_NEIGH_QUERY_BASE  = 30;
    constexpr uint64_t CYCLES_PER_NEIGHBOUR     = 6;
    constexpr uint64_t CYCLES_INTEGRATE_PER_P   = 20;
    constexpr uint64_t CYCLES_BOUNDARY_PER_P    = 12;
    constexpr uint64_t CYCLES_BUILD_HASH_PER_P  = 18;
}
