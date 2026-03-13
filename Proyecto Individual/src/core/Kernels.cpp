#include "core/Kernels.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Kernels {

// ============================================================
// Poly6 Kernel (2D)
//   W(r,h) = (4 / π h^8) * (h² - r²)^3   for 0 ≤ r ≤ h
//
//   Standard 2D normalisation.
// ============================================================
float poly6(float r, float h) {
    if (r < 0.0f || r > h) return 0.0f;
    float h2   = h * h;
    float r2   = r * r;
    float diff = h2 - r2;
    float coeff = 4.0f / (static_cast<float>(M_PI) * std::pow(h, 8));
    return coeff * diff * diff * diff;
}

// ============================================================
// Spiky Kernel Gradient (2D)
//   W(r,h) = (10 / π h^5) * (h - r)^3
//   ∇W(r,h) = -(30 / π h^5) * (h - r)^2 * (rij / r)
//   for 0 < r ≤ h
// ============================================================
Vec2 spikyGrad(const Vec2& rij, float r, float h) {
    if (r <= 1e-6f || r > h) return {0, 0};
    float coeff = -30.0f / (static_cast<float>(M_PI) * std::pow(h, 5));
    float diff  = h - r;
    float scale = coeff * diff * diff / r;
    return {rij.x * scale, rij.y * scale};
}

// ============================================================
// Viscosity Kernel Laplacian (2D)
//   ∇²W(r,h) = (40 / π h^5) * (h - r)
//   for 0 ≤ r ≤ h
// ============================================================
float viscosityLaplacian(float r, float h) {
    if (r < 0.0f || r > h) return 0.0f;
    float coeff = 40.0f / (static_cast<float>(M_PI) * std::pow(h, 5));
    return coeff * (h - r);
}

} // namespace Kernels
