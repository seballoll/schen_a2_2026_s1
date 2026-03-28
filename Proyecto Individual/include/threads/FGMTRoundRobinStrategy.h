#pragma once

#include "threads/ThreadStrategyContract.h"

namespace th {

class FGMTRoundRobinStrategy final : public ThreadStrategyContract {
public:
    explicit FGMTRoundRobinStrategy(int quantumItems = 16);

    std::string name() const override;
    void configure(int workers) override;
    void runForRange(int itemCount, const Task& task) override;
    void barrier() override;

    int quantumItems() const;

private:
    int workers_ = 1;
    int quantumItems_ = 16;
};

} // namespace th
