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
    constexpr float  DT                  = 0.001f;      // timestep (seconds)
    constexpr int    MAX_STEPS           = 50000;        // safety cap

    // --- Spatial Hash ---
    constexpr float  CELL_SIZE           = SMOOTHING_RADIUS;
    constexpr int    HASH_TABLE_SIZE     = 10007;

    // --- Threading defaults ---
    constexpr int    DEFAULT_THREAD_COUNT = 4;

    // --- Boundary ---
    constexpr float  BOUNDARY_DAMPING    = -0.5f;
}
