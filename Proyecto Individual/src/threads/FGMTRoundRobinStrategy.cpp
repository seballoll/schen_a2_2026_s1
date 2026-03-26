#include "threads/FGMTRoundRobinStrategy.h"

#include <algorithm>

namespace th {

FGMTRoundRobinStrategy::FGMTRoundRobinStrategy(int quantumItems)
    : quantumItems_(std::max(1, quantumItems)) {}

std::string FGMTRoundRobinStrategy::name() const {
    return "FGMT-RoundRobin";
}

void FGMTRoundRobinStrategy::configure(int workers) {
    workers_ = std::max(1, workers);
}

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

void FGMTRoundRobinStrategy::barrier() {
    // No-op in simulated FGMT: switching happens during runForRange.
}

int FGMTRoundRobinStrategy::quantumItems() const {
    return quantumItems_;
}

} // namespace th
