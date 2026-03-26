#pragma once

#include <vector>

namespace sim {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2 operator+(const Vec2& rhs) const { return {x + rhs.x, y + rhs.y}; }
    Vec2 operator-(const Vec2& rhs) const { return {x - rhs.x, y - rhs.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
};

struct Particle {
    Vec2 position{};
    Vec2 velocity{};
    Vec2 force{};
    float density = 0.0f;
    float pressure = 0.0f;
};

struct SPHParams {
    float domainWidth = 1.0f;
    float domainHeight = 1.0f;
    float particleSpacing = 0.02f;

    float restDensity = 1000.0f;
    float gasConstant = 2000.0f;
    float viscosity = 0.1f;
    float gravity = -9.8f;
    float particleMass = 1.0f;
    float boundaryDamping = -0.5f;
};

struct SPHState {
    SPHParams params{};
    std::vector<Particle> particles{};
};

void initializeDamBreak(SPHState& state, int numParticles);

} // namespace sim
