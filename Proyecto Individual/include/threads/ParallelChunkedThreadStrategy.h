#pragma once

#include "threads/ThreadStrategyContract.h"

namespace th {

class ParallelChunkedThreadStrategy final : public ThreadStrategyContract {
public:
    std::string name() const override;
    void configure(int workers) override;
    void runForRange(int itemCount, const Task& task) override;
    void barrier() override;

private:
    int workers_ = 1;
    bool shouldYieldOnStall(int workerId, int cursor) const;
};

} // namespace th
