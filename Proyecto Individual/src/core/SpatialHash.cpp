#include "core/SpatialHash.h"
#include <cmath>

SpatialHash::SpatialHash(float cellSize, int tableSize)
    : cellSize_(cellSize), tableSize_(tableSize) {}

// ============================================================
// hash: map (cx, cy) → bucket index using a prime-based hash
// ============================================================
int64_t SpatialHash::hashCell(int cx, int cy) const {
    // Large primes to spread values
    int64_t h = (static_cast<int64_t>(cx) * 73856093LL) ^
                (static_cast<int64_t>(cy) * 19349663LL);
    return ((h % tableSize_) + tableSize_) % tableSize_;
}

void SpatialHash::cellCoord(const Vec2& pos, int& cx, int& cy) const {
    cx = static_cast<int>(std::floor(pos.x / cellSize_));
    cy = static_cast<int>(std::floor(pos.y / cellSize_));
}

// ============================================================
// build — clear and re-insert every particle
// ============================================================
void SpatialHash::build(const std::vector<Particle>& particles) {
    buckets_.clear();
    for (int i = 0; i < static_cast<int>(particles.size()); ++i) {
        int cx, cy;
        cellCoord(particles[i].position, cx, cy);
        int64_t key = hashCell(cx, cy);
        buckets_[key].push_back(i);
    }
}

// ============================================================
// queryNeighbours — check 3×3 neighbourhood of cells
// ============================================================
void SpatialHash::queryNeighbours(const Vec2& pos, float radius,
                                  const std::vector<Particle>& particles,
                                  std::vector<int>& outIndices) const {
    outIndices.clear();
    int cx, cy;
    cellCoord(pos, cx, cy);
    float r2 = radius * radius;

    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            int64_t key = hashCell(cx + dx, cy + dy);
            auto it = buckets_.find(key);
            if (it == buckets_.end()) continue;
            for (int idx : it->second) {
                Vec2 diff = particles[idx].position - pos;
                if (diff.lengthSq() < r2) {
                    outIndices.push_back(idx);
                }
            }
        }
    }
}
