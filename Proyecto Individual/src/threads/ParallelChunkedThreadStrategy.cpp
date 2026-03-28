#include "threads/ParallelChunkedThreadStrategy.h"

#include <algorithm>
#include <vector>

namespace th {

// ======== MODEL IDENTIFICATION ========
std::string ParallelChunkedThreadStrategy::name() const {
    return "ParallelChunked";
}

// ======== CONFIGURATION ========
void ParallelChunkedThreadStrategy::configure(int workers) {
    workers_ = std::max(1, workers);
}

// ======== EXECUTION POLICY: COARSE CHUNKS + COOPERATIVE YIELD ========
void ParallelChunkedThreadStrategy::runForRange(int itemCount, const Task& task) {
    if (itemCount <= 0) return;

    // CGMT simulation: static coarse partitioning with cooperative switching on stall.
    const int workerCount = std::min(std::max(1, workers_), itemCount);
    const int chunk = (itemCount + workerCount - 1) / workerCount;

    std::vector<int> beginByWorker(static_cast<std::size_t>(workerCount), 0);
    std::vector<int> endByWorker(static_cast<std::size_t>(workerCount), 0);
    std::vector<int> cursorByWorker(static_cast<std::size_t>(workerCount), 0);

    for (int workerId = 0; workerId < workerCount; ++workerId) {
        const int begin = workerId * chunk;
        const int end = std::min(itemCount, begin + chunk);
        if (begin >= end) break;

        beginByWorker[static_cast<std::size_t>(workerId)] = begin;
        endByWorker[static_cast<std::size_t>(workerId)] = end;
        cursorByWorker[static_cast<std::size_t>(workerId)] = begin;
    }

    int activeWorkers = 0;
    for (int workerId = 0; workerId < workerCount; ++workerId) {
        if (cursorByWorker[static_cast<std::size_t>(workerId)] < endByWorker[static_cast<std::size_t>(workerId)]) {
            ++activeWorkers;
        }
    }

    int currentWorker = 0;
    while (activeWorkers > 0) {
        int& cursor = cursorByWorker[static_cast<std::size_t>(currentWorker)];
        const int end = endByWorker[static_cast<std::size_t>(currentWorker)];

        if (cursor < end) {
            bool yieldedByStall = false;

            while (cursor < end) {
                const int segmentStart = cursor;
                const int remaining = end - cursor;

                int segmentLen = remaining;
                if (remaining > 1 && shouldYieldOnStall(currentWorker, cursor)) {
                    // Coarse-grained: run a sizable segment before yielding due to a stall.
                    segmentLen = std::min(remaining, std::max(8, chunk / 3));
                    yieldedByStall = true;
                }

                const int segmentEnd = segmentStart + segmentLen;
                task(segmentStart, segmentEnd, currentWorker);
                cursor = segmentEnd;

                if (yieldedByStall) {
                    break;
                }
            }

            if (cursor >= end) {
                --activeWorkers;
            }
        }

        currentWorker = (currentWorker + 1) % workerCount;
    }
}

// ======== BARRIER SEMANTICS ========
void ParallelChunkedThreadStrategy::barrier() {
    // No-op in simulated CGMT.
}

// ======== SYNTHETIC STALL SIGNAL ========
bool ParallelChunkedThreadStrategy::shouldYieldOnStall(int workerId, int cursor) const {
    // Deterministic synthetic stall event used only for controlled scheduler simulation.
    const int signature = (cursor / 8) + workerId * 5 + workers_ * 3;
    return (signature % 11) == 0;
}

} // namespace th
