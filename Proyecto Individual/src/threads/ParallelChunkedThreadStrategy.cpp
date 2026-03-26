#include "threads/ParallelChunkedThreadStrategy.h"

#include <algorithm>

namespace th {

std::string ParallelChunkedThreadStrategy::name() const {
    return "ParallelChunked";
}

void ParallelChunkedThreadStrategy::configure(int workers) {
    workers_ = std::max(1, workers);
}

void ParallelChunkedThreadStrategy::runForRange(int itemCount, const Task& task) {
    if (itemCount <= 0) return;

    // CGMT simulation: static coarse partitioning without creating OS threads.
    const int workerCount = std::min(std::max(1, workers_), itemCount);
    const int chunk = (itemCount + workerCount - 1) / workerCount;

    for (int workerId = 0; workerId < workerCount; ++workerId) {
        const int begin = workerId * chunk;
        const int end = std::min(itemCount, begin + chunk);
        if (begin >= end) break;

        task(begin, end, workerId);
    }
}

void ParallelChunkedThreadStrategy::barrier() {
    // No-op in simulated CGMT.
}

} // namespace th
