#include "threads/ParallelChunkedThreadStrategy.h"

#include <algorithm>
#include <thread>
#include <vector>

namespace th {

std::string ParallelChunkedThreadStrategy::name() const {
    return "ParallelChunked";
}

void ParallelChunkedThreadStrategy::configure(int workers) {
    workers_ = std::max(1, workers);
}

void ParallelChunkedThreadStrategy::runForRange(int itemCount, const Task& task) {
    if (itemCount <= 0) return;

    const int workerCount = std::min(std::max(1, workers_), itemCount);
    const int chunk = (itemCount + workerCount - 1) / workerCount;

    std::vector<std::thread> pool;
    pool.reserve(static_cast<std::size_t>(workerCount));

    for (int workerId = 0; workerId < workerCount; ++workerId) {
        const int begin = workerId * chunk;
        const int end = std::min(itemCount, begin + chunk);
        if (begin >= end) break;

        pool.emplace_back([&, begin, end, workerId]() {
            task(begin, end, workerId);
        });
    }

    for (auto& worker : pool) {
        worker.join();
    }
}

void ParallelChunkedThreadStrategy::barrier() {
    // Per-stage implementation already joins workers.
}

} // namespace th
