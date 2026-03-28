#include "threads/FGMTRoundRobinStrategy.h"

#include <algorithm>

namespace th {

// ======== CONSTRUCTOR / QUANTUM SETUP ========
FGMTRoundRobinStrategy::FGMTRoundRobinStrategy(int quantumItems)
    : quantumItems_(std::max(1, quantumItems)) {}

// ======== MODEL IDENTIFICATION ========
std::string FGMTRoundRobinStrategy::name() const {
    return "FGMT-RoundRobin";
}

// ======== CONFIGURATION ========
void FGMTRoundRobinStrategy::configure(int workers) {
    workers_ = std::max(1, workers);
}

// ======== EXECUTION POLICY: ROUND-ROBIN QUANTA ========
void FGMTRoundRobinStrategy::runForRange(int itemCount, const Task& task) {
    if (itemCount <= 0) return;

    int nextBegin = 0;
    int currentWorker = 0;

    while (nextBegin < itemCount) {
        const int begin = nextBegin;
        const int end = std::min(itemCount, begin + quantumItems_);

        task(begin, end, currentWorker);

        nextBegin = end;
        currentWorker = (currentWorker + 1) % workers_;
    }
}

// ======== BARRIER SEMANTICS ========
void FGMTRoundRobinStrategy::barrier() {
    // No-op in simulated FGMT: switching happens during runForRange.
}

// ======== MODEL PARAMETERS ========
int FGMTRoundRobinStrategy::quantumItems() const {
    return quantumItems_;
}

} // namespace th
