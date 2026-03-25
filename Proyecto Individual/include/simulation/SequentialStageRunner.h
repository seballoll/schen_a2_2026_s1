#pragma once

#include "simulation/StageRunnerContract.h"

namespace sim {

class SequentialStageRunner final : public StageRunnerContract {
public:
    SequentialStageRunner() = default;

    void initialize(const RunConfig& config) override;
    void runStage(StageContext& context) override;
    void runStep(int stepIndex) override;
    void runAllSteps() override;

    const std::vector<StageMetrics>& stageMetrics() const override;
    const RunMetrics& runMetrics() const override;
    String modelName() const override;

private:
    RunConfig config_{};
    RunMetrics runMetrics_{};
    bool initialized_ = false;

    void resetMetrics();
    StageMetrics& metricsFor(StageId stage);
    void executeStage(StageId stage, int stepIndex);

    U64 baseWorkCycles(StageId stage) const;
    U64 stageWorkCycles(StageId stage) const;
    U64 stageStallCycles(StageId stage, int stepIndex) const;
};

} // namespace sim
