#include "threads/SequentialThreadStrategy.h"

#include <algorithm>

namespace th {

// ======== MODEL IDENTIFICATION ========
std::string SequentialThreadStrategy::name() const {
    return "Sequential";
}

// ======== CONFIGURATION ========
void SequentialThreadStrategy::configure(int workers) {
    workers_ = std::max(1, workers);
}

// ======== EXECUTION POLICY: SINGLE WORKER ========
void SequentialThreadStrategy::runForRange(int itemCount, const Task& task) {
    if (itemCount <= 0) return;
    task(0, itemCount, 0);
}

// ======== BARRIER SEMANTICS ========
void SequentialThreadStrategy::barrier() {
    // No-op in sequential execution.
}

} // namespace th
