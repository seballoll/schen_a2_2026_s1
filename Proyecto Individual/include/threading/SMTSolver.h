#pragma once

#include "core/SPHSolver.h"
#include <thread>
#include <functional>

// ============================================================
// Simultaneous Multithreading (SMT / Hyper-Threading)
//
// Concept: a single physical core runs multiple hardware
// threads sharing execution resources.  We model this by
// pinning threads to the same physical core using CPU
// affinity (Linux sched_setaffinity).
//
// On an Intel CPU with HT the OS exposes 2 logical cores per
// physical core.  We detect topology and schedule our threads
// to share physical cores, measuring the ILP benefit.
//
// Key metric: does SMT give speedup beyond the physical core
// count?
// ============================================================
class SMTSolver : public SPHSolver {
public:
    explicit SMTSolver(int numThreads);
    std::string modelName() const override;

protected:
    void computeDensityPressure() override;
    void computeForces()         override;
    void integrate(float dt)     override;
    void enforceBoundary()       override;

private:
    int numThreads_;
    void parallelForSMT(int n, std::function<void(int, int)> func);

    /// Try to pin thread `tid` to a logical core that shares a
    /// physical core with another thread (SMT-style scheduling).
    static void pinToLogicalCore(int tid);
};
