#pragma once

// ============================================================
// Global simulation parameters — tweak these to experiment
// ============================================================

namespace Config {

    // --- Window / Rendering ---
    constexpr int    WINDOW_WIDTH        = 1200;
    constexpr int    WINDOW_HEIGHT       = 800;
    constexpr float  PARTICLE_RENDER_RADIUS = 3.0f;

    // --- Domain ---
    constexpr float  DOMAIN_WIDTH        = 120.0f;   // simulation metres
    constexpr float  DOMAIN_HEIGHT       = 80.0f;

    // --- SPH Parameters ---
    constexpr int    NUM_PARTICLES       = 2000;      // total particles
    constexpr float  PARTICLE_MASS       = 1.0f;
    constexpr float  REST_DENSITY        = 1000.0f;   // kg/m^3 (water)
    constexpr float  GAS_CONSTANT        = 2000.0f;   // stiffness (Tait EOS)
    constexpr float  SMOOTHING_RADIUS    = 2.5f;      // h — kernel support radius
    constexpr float  VISCOSITY           = 250.0f;     // dynamic viscosity μ
    constexpr float  GRAVITY             = -9.81f;     // m/s^2

    // --- Time Integration ---
    constexpr float  DT                  = 0.0005f;   // timestep (seconds)
    constexpr int    MAX_STEPS           = 10000;      // safety cap

    // --- Spatial Hash ---
    constexpr float  CELL_SIZE           = SMOOTHING_RADIUS;  // cell ≥ h
    constexpr int    HASH_TABLE_SIZE     = 10007;             // prime for hashing

    // --- Threading defaults ---
    constexpr int    DEFAULT_THREAD_COUNT = 4;

    // --- Boundary ---
    constexpr float  BOUNDARY_DAMPING    = -0.5f;     // velocity factor on wall hit
}
