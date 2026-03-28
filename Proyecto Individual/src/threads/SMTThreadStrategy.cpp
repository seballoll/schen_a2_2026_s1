#include "threads/SMTThreadStrategy.h"

#include <algorithm>
#include <atomic>
#include <thread>
#include <vector>

namespace th {

// ======== MODEL IDENTIFICATION ========
std::string SMTThreadStrategy::name() const {
    return "SMT-RealThreads";
}

// ======== CONFIGURATION ========
void SMTThreadStrategy::configure(int workers) {
    workers_ = std::max(1, workers);
}

// ======== EXECUTION POLICY: DYNAMIC WORK-STEAL LIKE CHUNKING ========
void SMTThreadStrategy::runForRange(int itemCount, const Task& task) {
    if (itemCount <= 0) return;

    const int workerCount = std::min(std::max(1, workers_), itemCount);
    std::atomic<int> nextBegin{0};

    std::vector<std::thread> pool;
    pool.reserve(static_cast<std::size_t>(workerCount));

    for (int workerId = 0; workerId < workerCount; ++workerId) {
        pool.emplace_back([&, workerId]() {
            while (true) {
                const int begin = nextBegin.fetch_add(grainSize_, std::memory_order_relaxed);
                if (begin >= itemCount) {
                    break;
                }
                const int end = std::min(itemCount, begin + grainSize_);
                task(begin, end, workerId);
            }
        });
    }

    for (auto& worker : pool) {
        worker.join();
    }
}

// ======== BARRIER SEMANTICS ========
void SMTThreadStrategy::barrier() {
    // Per-stage join in runForRange already acts as a barrier.
}

} // namespace th
