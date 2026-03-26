#include "threads/SequentialThreadStrategy.h"

#include <algorithm>

namespace th {

std::string SequentialThreadStrategy::name() const {
    return "Sequential";
}

void SequentialThreadStrategy::configure(int workers) {
    workers_ = std::max(1, workers);
}

void SequentialThreadStrategy::runForRange(int itemCount, const Task& task) {
    if (itemCount <= 0) return;
    task(0, itemCount, 0);
}

void SequentialThreadStrategy::barrier() {
    // No-op in sequential execution.
}

} // namespace th
