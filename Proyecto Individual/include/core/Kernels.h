#pragma once

#include "Vec2.h"

// ============================================================
// SPH Smoothing Kernels (2D)
//
// We use the Poly6 kernel for density estimation and the
// Spiky kernel gradient for pressure forces, plus the
// Laplacian of the viscosity kernel.
//
// All kernels use smoothing length `h`.
// ============================================================
namespace Kernels {

    /// Poly6 kernel W(r, h) — used for density
    float poly6(float r, float h);

    /// Gradient of Spiky kernel — used for pressure force
    Vec2 spikyGrad(const Vec2& rij, float r, float h);

    /// Laplacian of viscosity kernel — used for viscosity force
    float viscosityLaplacian(float r, float h);

} // namespace Kernels
