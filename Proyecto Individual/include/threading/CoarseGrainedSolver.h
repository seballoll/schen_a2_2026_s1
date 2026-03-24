#pragma once

#include "core/SPHSolver.h"

#include <cstdint>
#include <functional>
#include <vector>

// ============================================================
// Coarse-Grained Multithreading (CGMT)
//
// Concept (PROJECT MODEL): emulate hardware coarse-grained MT
// with a cooperative scheduler that runs a thread until it hits
// a long-latency stall, then switches.
//
// IMPORTANT: This is intentionally NOT implemented with
// std::thread. We simulate stalls/switching to report cycles.
// ============================================================
class CoarseGrainedSolver : public SPHSolver {
public:
    explicit CoarseGrainedSolver(int numThreads);
    std::string modelName() const override;

protected:
    void computeDensityPressure() override;
    void computeForces()         override;
    void integrate(float dt)     override;
    void enforceBoundary()       override;

private:
    int numThreads_;

    void runStageCGMT(
        int stageId,
        int actualThreads,
        const std::function<void(int /*tid*/, int /*particleIndex*/, std::vector<int>& /*neighboursScratch*/)>& particleWork,
        const std::function<uint64_t(int /*tid*/, int /*particleIndex*/, const std::vector<int>& /*neighboursScratch*/)>& cycleCost);
};
