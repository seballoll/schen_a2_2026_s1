#pragma once

#include "core/Particle.h"
#include <vector>
#include <unordered_map>
#include <cstdint>

// ============================================================
// Spatial Hash Grid — O(1) average-case neighbour lookup
//
// Each cell maps to a bucket of particle indices.
// The hash uses a prime-based scheme on integer grid coords.
// ============================================================
class SpatialHash {
public:
    SpatialHash(float cellSize, int tableSize);

    /// Rebuild the entire grid from scratch (call once per timestep).
    void build(const std::vector<Particle>& particles);

    /// Return indices of all particles within `radius` of `pos`.
    void queryNeighbours(const Vec2& pos, float radius,
                         const std::vector<Particle>& particles,
                         std::vector<int>& outIndices) const;

private:
    float cellSize_;
    int   tableSize_;

    // bucket: hash → list of particle indices
    std::unordered_map<int64_t, std::vector<int>> buckets_;

    int64_t hashCell(int cx, int cy) const;
    void    cellCoord(const Vec2& pos, int& cx, int& cy) const;
};
