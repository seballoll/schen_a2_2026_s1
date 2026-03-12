#pragma once

#include "Vec2.h"

// ============================================================
// Single SPH particle
// ============================================================
struct Particle {
    Vec2  position;
    Vec2  velocity;
    Vec2  force;          // accumulated force this timestep

    float density   = 0.0f;
    float pressure  = 0.0f;

    Particle() = default;
    Particle(Vec2 pos, Vec2 vel = {0, 0})
        : position(pos), velocity(vel) {}
};
