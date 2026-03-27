#pragma once

#include "threads/ThreadStrategyContract.h"

namespace th {

class CMPThreadStrategy final : public ThreadStrategyContract {
public:
    std::string name() const override;
    void configure(int workers) override;
    void runForRange(int itemCount, const Task& task) override;
    void barrier() override;

private:
    int workers_ = 1;
};

} // namespace th
