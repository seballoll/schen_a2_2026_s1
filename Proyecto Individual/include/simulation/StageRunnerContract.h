#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sim {

using CycleCount = std::uint64_t;

enum class StageId {
    Setup = 0,
    NeighbourStructure,
    DensityPressure,
    Forces,
    Integrate,
    Boundary,
    Metrics
};

struct RunConfig {
    int steps = 0;
    int particles = 0;
    int threads = 1;
    float dt = 0.0f;
    CycleCount seed = 0;
};

struct StageContext {
    int stepIndex = 0;
    StageId stage = StageId::Setup;
    const RunConfig* config = nullptr;
};

struct StageMetrics {
    StageId stage = StageId::Setup;
    std::string stageName;

    double wallMs = 0.0;
    CycleCount totalCycles = 0;
    CycleCount workCycles = 0;
    CycleCount idleCycles = 0;
    CycleCount contextSwitchCycles = 0;
    CycleCount stallHiddenCycles = 0;
    CycleCount stallExposedCycles = 0;
};

struct RunMetrics {
    double totalWallMs = 0.0;
    CycleCount totalCycles = 0;
    std::vector<StageMetrics> perStage;
};

class StageRunnerContract {
public:
    virtual ~StageRunnerContract() = default;

    // Setup inicial. Debe dejar buffers y estado listos para correr.
    virtual void initialize(const RunConfig& config) = 0;

    // Ejecuta una etapa puntual de un step.
    virtual void runStage(StageContext& context) = 0;

    // Ejecuta un step completo respetando el pipeline unificado.
    virtual void runStep(int stepIndex) = 0;

    // Ejecuta toda la corrida.
    virtual void runAllSteps() = 0;

    // Reportes para tablas por etapa y resumen global.
    virtual const std::vector<StageMetrics>& stageMetrics() const = 0;
    virtual const RunMetrics& runMetrics() const = 0;

    // Nombre legible del modelo (Sequential, FGMT, CGMT, SMT, CMP).
    virtual std::string modelName() const = 0;
};

inline const char* stageName(StageId id) {
    switch (id) {
        case StageId::Setup: return "Setup";
        case StageId::NeighbourStructure: return "NeighbourStructure";
        case StageId::DensityPressure: return "DensityPressure";
        case StageId::Forces: return "Forces";
        case StageId::Integrate: return "Integrate";
        case StageId::Boundary: return "Boundary";
        case StageId::Metrics: return "Metrics";
    }
    return "Unknown";
}

inline bool requiresBarrier(StageId id) {
    return id == StageId::NeighbourStructure ||
           id == StageId::DensityPressure ||
           id == StageId::Forces ||
           id == StageId::Integrate ||
           id == StageId::Boundary;
}

inline std::vector<StageId> defaultPipeline() {
    return {
        StageId::NeighbourStructure,
        StageId::DensityPressure,
        StageId::Forces,
        StageId::Integrate,
        StageId::Boundary,
        StageId::Metrics
    };
}

} // namespace sim
