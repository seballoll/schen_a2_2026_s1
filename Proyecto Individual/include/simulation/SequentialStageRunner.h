#pragma once

#include "simulation/StageRunnerContract.h"
#include "simulation/SPHDataModel.h"

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
    std::string modelName() const override;
    const SPHState& state() const;

private:
    RunConfig config_{};
    RunMetrics runMetrics_{};
    SPHState state_{};
    bool initialized_ = false;

    void resetMetrics();
    StageMetrics& stageMetricsRef(StageId stage);
    void executeStageSynthetic(StageId stage, int stepIndex);
    void executeDensityPressureReal(StageMetrics& metrics, int stepIndex);

    CycleCount baseCyclesPerParticle(StageId stage) const;
    CycleCount estimateWorkCycles(StageId stage) const;
    CycleCount estimateExposedStallCycles(StageId stage, int stepIndex) const;
};

} // namespace sim
