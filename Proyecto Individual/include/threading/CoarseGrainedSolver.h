#pragma once

#include "core/SPHSolver.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

// ============================================================
// Coarse-Grained Multithreading (CGMT)
//
// Concept: threads run large chunks of work and only
// synchronise at long-latency events (here: between pipeline
// stages).  Unlike FGMT where we sync per-sub-step, CGMT
// gives each thread a big block and waits only at stage
// boundaries.
//
// Analogy to hardware CGMT: the processor switches threads
// only on cache misses or long stalls, not every cycle.
// ============================================================
class CoarseGrainedSolver : public SPHSolver {
public:
    explicit CoarseGrainedSolver(int numThreads);
    std::string modelName() const override;

    /// Override step() to launch persistent worker threads that
    /// process ALL stages in one go with barriers in between.
    void stepParallel(float dt);

protected:
    void computeDensityPressure() override;
    void computeForces()         override;
    void integrate(float dt)     override;
    void enforceBoundary()       override;

private:
    int numThreads_;
    void parallelFor(int n, std::function<void(int, int)> func);
};
