#pragma once

#include "core/SPHSolver.h"
#include <thread>
#include <mutex>
#include <barrier>
#include <functional>

// ============================================================
// Fine-Grained Multithreading (FGMT)
//
// Concept: threads are interleaved at a very fine granularity.
// On every pipeline stage we split the particle array evenly
// among threads.  Between stages we use a BARRIER so that no
// thread advances until all finish the current stage.
//
// Analogy to hardware FGMT: the processor switches between
// threads on every cycle to hide latency.  Here we model that
// by having many short parallel sections with frequent syncs.
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

    /// Helper: run `func(startIdx, endIdx)` on `numThreads_` threads,
    /// splitting particles evenly.
    void parallelFor(int n, std::function<void(int, int)> func);
};
