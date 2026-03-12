#pragma once

#include "core/SPHSolver.h"

// ============================================================
// SequentialSolver — baseline single-threaded execution
// ============================================================
class SequentialSolver : public SPHSolver {
public:
    std::string modelName() const override { return "Sequential (Single-Thread)"; }

    // Uses the base-class implementations directly (already sequential).
};
