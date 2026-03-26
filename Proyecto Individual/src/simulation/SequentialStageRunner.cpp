#include "simulation/SequentialStageRunner.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace sim {

namespace {

float poly6Kernel(float distanceSquared, float smoothingRadius) {
    const float h2 = smoothingRadius * smoothingRadius;
    if (distanceSquared >= h2) return 0.0f;

    const float x = h2 - distanceSquared;
    const float pi = 3.14159265359f;
    const float h9 = std::pow(smoothingRadius, 9.0f);
    const float coeff = 315.0f / (64.0f * pi * h9);
    return coeff * x * x * x;
}

float spikyGradFactor(float distance, float smoothingRadius) {
    if (distance <= 1e-6f || distance >= smoothingRadius) return 0.0f;

    const float x = smoothingRadius - distance;
    const float pi = 3.14159265359f;
    const float h6 = std::pow(smoothingRadius, 6.0f);
    const float coeff = -45.0f / (pi * h6);
    return coeff * x * x;
}

float viscosityLaplacian(float distance, float smoothingRadius) {
    if (distance >= smoothingRadius) return 0.0f;

    const float pi = 3.14159265359f;
    const float h6 = std::pow(smoothingRadius, 6.0f);
    const float coeff = 45.0f / (pi * h6);
    return coeff * (smoothingRadius - distance);
}

} // namespace

void SequentialStageRunner::initialize(const RunConfig& config) {
    config_ = config;

    if (config_.steps <= 0) config_.steps = 1;
    if (config_.particles <= 0) config_.particles = 1;
    if (config_.threads <= 0) config_.threads = 1;

    state_ = {};
    initializeDamBreak(state_, config_.particles);

    resetMetrics();
    initialized_ = true;
}

void SequentialStageRunner::runStage(StageContext& context) {
    if (!initialized_) {
        throw std::runtime_error("SequentialStageRunner no fue inicializado");
    }
    executeStageSynthetic(context.stage, context.stepIndex);
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

    CycleCount accumulatedCycles = 0;
    for (const auto& stageMetrics : runMetrics_.perStage) {
        accumulatedCycles += stageMetrics.totalCycles;
    }
    runMetrics_.totalCycles = accumulatedCycles;
}

const std::vector<StageMetrics>& SequentialStageRunner::stageMetrics() const {
    return runMetrics_.perStage;
}

const RunMetrics& SequentialStageRunner::runMetrics() const {
    return runMetrics_;
}

const SPHState& SequentialStageRunner::state() const {
    return state_;
}

std::string SequentialStageRunner::modelName() const {
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

StageMetrics& SequentialStageRunner::stageMetricsRef(StageId stage) {
    for (auto& m : runMetrics_.perStage) {
        if (m.stage == stage) return m;
    }
    throw std::runtime_error("Etapa no encontrada en stageMetricsRef");
}

void SequentialStageRunner::executeStageSynthetic(StageId stage, int stepIndex) {
    auto& m = stageMetricsRef(stage);

    if (stage == StageId::DensityPressure) {
        executeDensityPressureReal(m, stepIndex);
        return;
    }

    if (stage == StageId::Forces) {
        executeForcesReal(m, stepIndex);
        return;
    }

    const auto t0 = std::chrono::steady_clock::now();

    const CycleCount workCycles = estimateWorkCycles(stage);
    const CycleCount exposedStallCycles = estimateExposedStallCycles(stage, stepIndex);

    // Trabajo sintetico minimo para que wall time no sea siempre cero.
    volatile CycleCount sink = 0;
    const int particleCount = static_cast<int>(state_.particles.size());
    const int iterations = particleCount / 4 + 1;
    for (int i = 0; i < iterations; ++i) {
        sink += static_cast<CycleCount>(i + stepIndex + static_cast<int>(stage));
    }
    (void)sink;

    const auto t1 = std::chrono::steady_clock::now();

    m.wallMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
    m.workCycles += workCycles;
    m.idleCycles += exposedStallCycles;
    m.stallExposedCycles += exposedStallCycles;
    m.totalCycles += workCycles + exposedStallCycles;
}

void SequentialStageRunner::executeDensityPressureReal(StageMetrics& metrics, int stepIndex) {
    const auto t0 = std::chrono::steady_clock::now();

    const float h = state_.params.particleSpacing * 2.0f;
    const float mass = state_.params.particleMass;
    const float restDensity = state_.params.restDensity;
    const float gasConstant = state_.params.gasConstant;

    CycleCount neighbourChecks = 0;
    CycleCount neighbourContributions = 0;

    const int n = static_cast<int>(state_.particles.size());
    for (int i = 0; i < n; ++i) {
        Particle& pi = state_.particles[static_cast<std::size_t>(i)];
        float density = 0.0f;

        for (int j = 0; j < n; ++j) {
            const Particle& pj = state_.particles[static_cast<std::size_t>(j)];
            const float dx = pi.position.x - pj.position.x;
            const float dy = pi.position.y - pj.position.y;
            const float r2 = dx * dx + dy * dy;

            ++neighbourChecks;
            const float w = poly6Kernel(r2, h);
            if (w > 0.0f) {
                ++neighbourContributions;
                density += mass * w;
            }
        }

        pi.density = std::max(density, restDensity * 0.01f);
        pi.pressure = gasConstant * (pi.density - restDensity);
    }

    const auto t1 = std::chrono::steady_clock::now();

    const CycleCount workCycles =
        neighbourChecks * 4 + neighbourContributions * 10 + static_cast<CycleCount>(n) * 12;
    const CycleCount exposedStallCycles = estimateExposedStallCycles(StageId::DensityPressure, stepIndex);

    metrics.wallMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
    metrics.workCycles += workCycles;
    metrics.idleCycles += exposedStallCycles;
    metrics.stallExposedCycles += exposedStallCycles;
    metrics.totalCycles += workCycles + exposedStallCycles;
}

void SequentialStageRunner::executeForcesReal(StageMetrics& metrics, int stepIndex) {
    const auto t0 = std::chrono::steady_clock::now();

    const float h = state_.params.particleSpacing * 2.0f;
    const float h2 = h * h;
    const float mass = state_.params.particleMass;
    const float viscosity = state_.params.viscosity;
    const float gravity = state_.params.gravity;
    const float densityFloor = state_.params.restDensity * 0.01f;

    CycleCount neighbourChecks = 0;
    CycleCount neighbourInteractions = 0;

    const int n = static_cast<int>(state_.particles.size());
    for (int i = 0; i < n; ++i) {
        Particle& pi = state_.particles[static_cast<std::size_t>(i)];

        float forcePressureX = 0.0f;
        float forcePressureY = 0.0f;
        float forceViscosityX = 0.0f;
        float forceViscosityY = 0.0f;

        for (int j = 0; j < n; ++j) {
            if (j == i) continue;

            const Particle& pj = state_.particles[static_cast<std::size_t>(j)];
            const float dx = pi.position.x - pj.position.x;
            const float dy = pi.position.y - pj.position.y;
            const float r2 = dx * dx + dy * dy;

            ++neighbourChecks;
            if (r2 >= h2) continue;

            const float r = std::sqrt(r2);
            if (r <= 1e-6f) continue;

            ++neighbourInteractions;

            const float densityJ = std::max(pj.density, densityFloor);
            const float invR = 1.0f / r;

            const float gradFactor = spikyGradFactor(r, h);
            const float gradWx = gradFactor * dx * invR;
            const float gradWy = gradFactor * dy * invR;

            const float pressureTerm = mass * (pi.pressure + pj.pressure) / (2.0f * densityJ);
            forcePressureX += -pressureTerm * gradWx;
            forcePressureY += -pressureTerm * gradWy;

            const float lap = viscosityLaplacian(r, h);
            forceViscosityX += viscosity * mass * (pj.velocity.x - pi.velocity.x) * lap / densityJ;
            forceViscosityY += viscosity * mass * (pj.velocity.y - pi.velocity.y) * lap / densityJ;
        }

        pi.force.x = forcePressureX + forceViscosityX;
        pi.force.y = forcePressureY + forceViscosityY + gravity * pi.density;
    }

    const auto t1 = std::chrono::steady_clock::now();

    const CycleCount workCycles =
        neighbourChecks * 6 + neighbourInteractions * 20 + static_cast<CycleCount>(n) * 16;
    const CycleCount exposedStallCycles = estimateExposedStallCycles(StageId::Forces, stepIndex);

    metrics.wallMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
    metrics.workCycles += workCycles;
    metrics.idleCycles += exposedStallCycles;
    metrics.stallExposedCycles += exposedStallCycles;
    metrics.totalCycles += workCycles + exposedStallCycles;
}

CycleCount SequentialStageRunner::baseCyclesPerParticle(StageId stage) const {
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

CycleCount SequentialStageRunner::estimateWorkCycles(StageId stage) const {
    return baseCyclesPerParticle(stage) * static_cast<CycleCount>(state_.particles.size());
}

CycleCount SequentialStageRunner::estimateExposedStallCycles(StageId stage, int stepIndex) const {
    if (stage != StageId::DensityPressure && stage != StageId::Forces) {
        return 0;
    }

    const CycleCount key =
        static_cast<CycleCount>(config_.seed + 31 * stepIndex + static_cast<int>(stage));
    const CycleCount events = (key % 5) + 1;
    return events * 120;
}

} // namespace sim
