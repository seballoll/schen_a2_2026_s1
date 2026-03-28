#include "simulation/SPHDataModel.h"

namespace sim {

void initializeDamBreak(SPHState& state, int numParticles) {
    if (numParticles <= 0) {
        state.particles.clear();
        return;
    }

    state.particles.clear();
    state.particles.reserve(static_cast<std::size_t>(numParticles));

    const float spacing = state.params.particleSpacing;
    const float startX = spacing;
    const float startY = spacing;

    int cols = static_cast<int>((state.params.domainWidth * 0.25f) / spacing);
    if (cols < 1) cols = 1;

    const int rows = (numParticles + cols - 1) / cols;

    int placed = 0;
    for (int r = 0; r < rows && placed < numParticles; ++r) {
        for (int c = 0; c < cols && placed < numParticles; ++c) {
            Particle p;
            p.position.x = startX + static_cast<float>(c) * spacing;
            p.position.y = startY + static_cast<float>(r) * spacing;
            p.velocity = {0.0f, 0.0f};
            p.force = {0.0f, 0.0f};
            p.density = state.params.restDensity;
            p.pressure = 0.0f;

            state.particles.push_back(p);
            ++placed;
        }
    }
}

} // namespace sim
