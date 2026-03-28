#pragma once

#include "simulation/StageRunnerContract.h"
#include "simulation/SPHDataModel.h"
#include "threads/ThreadStrategyContract.h"

#include <memory>
#include <vector>

namespace sim {

class SequentialStageRunner final : public StageRunnerContract {
public:
    struct StepMetricsSnapshot {
        int stepIndex = 0;
        double avgDensity = 0.0;
        double avgSpeed = 0.0;
        double totalKineticEnergy = 0.0;
        double maxSpeedObserved = 0.0;
        int invalidParticleCount = 0;
    };

    explicit SequentialStageRunner(std::unique_ptr<th::ThreadStrategyContract> strategy = nullptr);

    void initialize(const RunConfig& config) override;
    void runStage(StageContext& context) override;
    void runStep(int stepIndex) override;
    void runAllSteps() override;

    const std::vector<StageMetrics>& stageMetrics() const override;
    const RunMetrics& runMetrics() const override;
    std::string modelName() const override;
    const SPHState& state() const;
    const std::vector<StepMetricsSnapshot>& stepSnapshots() const;

private:
    RunConfig config_{};
    RunMetrics runMetrics_{};
    SPHState state_{};
    float gridCellSize_ = 0.0f;
    int gridWidth_ = 0;
    int gridHeight_ = 0;
    std::vector<std::vector<int>> gridBuckets_{};
    std::vector<StepMetricsSnapshot> stepSnapshots_{};
    std::unique_ptr<th::ThreadStrategyContract> strategy_{};
    bool initialized_ = false;

    void resetMetrics();
    StageMetrics& stageMetricsRef(StageId stage);
    void executeStageSynthetic(StageId stage, int stepIndex);
    void executeNeighbourStructureReal(StageMetrics& metrics);
    void executeDensityPressureReal(StageMetrics& metrics, int stepIndex);
    void executeForcesReal(StageMetrics& metrics, int stepIndex);
    void executeIntegrateReal(StageMetrics& metrics);
    void executeBoundaryReal(StageMetrics& metrics);
    void executeMetricsReal(StageMetrics& metrics, int stepIndex);

    int clampCellX(int cellX) const;
    int clampCellY(int cellY) const;
    int cellIndex(int cellX, int cellY) const;
    void neighbourCellBounds(const Particle& particle, int& minX, int& maxX, int& minY, int& maxY) const;
    int activeWorkersForCycles(int itemCount) const;
    void estimateStageStallBreakdown(
        StageId stage,
        int stepIndex,
        int itemCount,
        CycleCount candidateChecks,
        CycleCount interactions,
        CycleCount& contextSwitchCycles,
        CycleCount& hiddenStallCycles,
        CycleCount& exposedStallCycles) const;

    CycleCount baseCyclesPerParticle(StageId stage) const;
    CycleCount estimateWorkCycles(StageId stage) const;
    CycleCount estimateExposedStallCycles(StageId stage, int stepIndex) const;
};

} // namespace sim
