#include "simulation/SequentialStageRunner.h"

#include <chrono>
#include <stdexcept>

namespace sim {

void SequentialStageRunner::initialize(const RunConfig& config) {
    config_ = config;

    if (config_.steps <= 0) config_.steps = 1;
    if (config_.particles <= 0) config_.particles = 1;
    if (config_.threads <= 0) config_.threads = 1;

    resetMetrics();
    initialized_ = true;
}

void SequentialStageRunner::runStage(StageContext& context) {
    if (!initialized_) {
        throw std::runtime_error("SequentialStageRunner no fue inicializado");
    }
    executeStage(context.stage, context.stepIndex);
}

void SequentialStageRunner::runStep(int stepIndex) {
    StageContext context;
    context.stepIndex = stepIndex;
    context.config = &config_;

    const auto pipeline = defaultPipeline();
    for (StageId stage : pipeline) {
        context.stage = stage;
        runStage(context);
    }
}

void SequentialStageRunner::runAllSteps() {
    if (!initialized_) {
        throw std::runtime_error("SequentialStageRunner no fue inicializado");
    }

    const auto start = std::chrono::steady_clock::now();

    for (int step = 0; step < config_.steps; ++step) {
        runStep(step);
    }

    const auto end = std::chrono::steady_clock::now();
    runMetrics_.totalWallMs =
        std::chrono::duration<double, std::milli>(end - start).count();

    U64 sumCycles = 0;
    for (const auto& stage : runMetrics_.perStage) {
        sumCycles += stage.totalCycles;
    }
    runMetrics_.totalCycles = sumCycles;
}

const std::vector<StageMetrics>& SequentialStageRunner::stageMetrics() const {
    return runMetrics_.perStage;
}

const RunMetrics& SequentialStageRunner::runMetrics() const {
    return runMetrics_;
}

String SequentialStageRunner::modelName() const {
    return "Sequential";
}

void SequentialStageRunner::resetMetrics() {
    runMetrics_ = {};
    runMetrics_.perStage.clear();

    const auto pipeline = defaultPipeline();
    runMetrics_.perStage.reserve(pipeline.size());

    for (StageId stage : pipeline) {
        StageMetrics m;
        m.stage = stage;
        m.stageName = stageName(stage);
        runMetrics_.perStage.push_back(m);
    }
}

StageMetrics& SequentialStageRunner::metricsFor(StageId stage) {
    for (auto& m : runMetrics_.perStage) {
        if (m.stage == stage) return m;
    }
    throw std::runtime_error("Etapa no encontrada en metricsFor");
}

void SequentialStageRunner::executeStage(StageId stage, int stepIndex) {
    auto& m = metricsFor(stage);
    const auto t0 = std::chrono::steady_clock::now();

    const U64 work = stageWorkCycles(stage);
    const U64 stallExposed = stageStallCycles(stage, stepIndex);

    // Trabajo sintetico minimo para que wall time no sea siempre cero.
    volatile U64 sink = 0;
    const int iterations = config_.particles / 4 + 1;
    for (int i = 0; i < iterations; ++i) {
        sink += static_cast<U64>(i + stepIndex + static_cast<int>(stage));
    }
    (void)sink;

    const auto t1 = std::chrono::steady_clock::now();

    m.wallMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
    m.workCycles += work;
    m.idleCycles += stallExposed;
    m.stallExposedCycles += stallExposed;
    m.totalCycles += work + stallExposed;
}

U64 SequentialStageRunner::baseWorkCycles(StageId stage) const {
    switch (stage) {
        case StageId::NeighbourStructure: return 14;
        case StageId::DensityPressure: return 38;
        case StageId::Forces: return 52;
        case StageId::Integrate: return 18;
        case StageId::Boundary: return 12;
        case StageId::Metrics: return 4;
        case StageId::Setup: return 2;
    }
    return 0;
}

U64 SequentialStageRunner::stageWorkCycles(StageId stage) const {
    return baseWorkCycles(stage) * static_cast<U64>(config_.particles);
}

U64 SequentialStageRunner::stageStallCycles(StageId stage, int stepIndex) const {
    if (stage != StageId::DensityPressure && stage != StageId::Forces) {
        return 0;
    }

    const U64 key = static_cast<U64>(config_.seed + 31 * stepIndex + static_cast<int>(stage));
    const U64 events = (key % 5) + 1;
    return events * 120;
}

} // namespace sim
