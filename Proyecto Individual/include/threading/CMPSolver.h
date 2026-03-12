#pragma once

#include "core/SPHSolver.h"
#include <thread>
#include <vector>
#include <memory>
#include <functional>

// ============================================================
// Chip Multiprocessing (CMP)
//
// Concept: multiple independent cores, each with its own
// resources, running threads truly in parallel.  We model
// this by spawning threads and pinning each one to a DISTINCT
// physical core using CPU affinity.
//
// This represents the "embarrassingly parallel" ideal: every
// core works independently, syncing only at pipeline barriers.
//
// Key comparison: CMP vs SMT — do separate cores outperform
// shared-core hyper-threading?
// ============================================================
class CMPSolver : public SPHSolver {
public:
    explicit CMPSolver(int numThreads);
    std::string modelName() const override;

protected:
    void computeDensityPressure() override;
    void computeForces()         override;
    void integrate(float dt)     override;
    void enforceBoundary()       override;

private:
    int numThreads_;
    void parallelForCMP(int n, std::function<void(int, int)> func);

    /// Pin thread `tid` to a distinct physical core.
    static void pinToPhysicalCore(int tid);
};
