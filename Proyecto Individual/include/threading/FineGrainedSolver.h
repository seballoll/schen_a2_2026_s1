#pragma once

#include "core/SPHSolver.h"

#include <cstdint>
#include <functional>
#include <vector>

// ============================================================
// Fine-Grained Multithreading (FGMT)
//
// Concept (PROJECT MODEL): emulate hardware fine-grained MT
// with a *cooperative* scheduler and a small quantum.
//
// IMPORTANT: This is intentionally NOT implemented with
// std::thread. We simulate interleaving and stalls to report
// cycles/speedups per the assignment spec.
// ============================================================
class FineGrainedSolver : public SPHSolver {
public:
    explicit FineGrainedSolver(int numThreads);
    std::string modelName() const override;

protected:
    void computeDensityPressure() override;
    void computeForces()         override;
    void integrate(float dt)     override;
    void enforceBoundary()       override;

private:
    int numThreads_;

    void runStageFGMT(
        int stageId,
        int actualThreads,
        const std::function<void(int /*tid*/, int /*particleIndex*/, std::vector<int>& /*neighboursScratch*/)>& particleWork,
        const std::function<uint64_t(int /*tid*/, int /*particleIndex*/, const std::vector<int>& /*neighboursScratch*/)>& cycleCost);
};
