#include "simulation/SequentialStageRunner.h"
#include "threads/FGMTRoundRobinStrategy.h"
#include "threads/ParallelChunkedThreadStrategy.h"
#include "threads/CMPThreadStrategy.h"
#include "threads/SequentialThreadStrategy.h"
#include "threads/SMTThreadStrategy.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <limits>
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

SequentialStageRunner::SequentialStageRunner(std::unique_ptr<th::ThreadStrategyContract> strategy)
    : strategy_(std::move(strategy)) {
    if (!strategy_) {
        strategy_ = std::make_unique<th::SequentialThreadStrategy>();
    }
}

void SequentialStageRunner::initialize(const RunConfig& config) {
    config_ = config;

    if (config_.steps <= 0) config_.steps = 1;
    if (config_.particles <= 0) config_.particles = 1;
    if (config_.threads <= 0) config_.threads = 1;

    state_ = {};
    initializeDamBreak(state_, config_.particles);
    gridCellSize_ = std::max(state_.params.smoothingRadius, 1e-6f);
    gridWidth_ = std::max(1, static_cast<int>(std::ceil(state_.params.domainWidth / gridCellSize_)));
    gridHeight_ = std::max(1, static_cast<int>(std::ceil(state_.params.domainHeight / gridCellSize_)));
    gridBuckets_.assign(static_cast<std::size_t>(gridWidth_ * gridHeight_), {});
    stepSnapshots_.clear();
    stepSnapshots_.reserve(static_cast<std::size_t>(config_.steps));

    strategy_->configure(config_.threads);

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

const std::vector<SequentialStageRunner::StepMetricsSnapshot>&
SequentialStageRunner::stepSnapshots() const {
    return stepSnapshots_;
}

std::string SequentialStageRunner::modelName() const {
    return strategy_ ? strategy_->name() : "Sequential";
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

    if (stage == StageId::NeighbourStructure) {
        executeNeighbourStructureReal(m);
        return;
    }

    if (stage == StageId::DensityPressure) {
        executeDensityPressureReal(m, stepIndex);
        return;
    }

    if (stage == StageId::Forces) {
        executeForcesReal(m, stepIndex);
        return;
    }

    if (stage == StageId::Integrate) {
        executeIntegrateReal(m);
        return;
    }

    if (stage == StageId::Boundary) {
        executeBoundaryReal(m);
        return;
    }

    if (stage == StageId::Metrics) {
        executeMetricsReal(m, stepIndex);
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

void SequentialStageRunner::executeNeighbourStructureReal(StageMetrics& metrics) {
    const auto t0 = std::chrono::steady_clock::now();

    const int n = static_cast<int>(state_.particles.size());

    for (auto& bucket : gridBuckets_) {
        bucket.clear();
    }

    CycleCount bucketInserts = 0;
    for (int i = 0; i < n; ++i) {
        const Particle& p = state_.particles[static_cast<std::size_t>(i)];
        int cx = static_cast<int>(std::floor(p.position.x / gridCellSize_));
        int cy = static_cast<int>(std::floor(p.position.y / gridCellSize_));
        cx = clampCellX(cx);
        cy = clampCellY(cy);
        gridBuckets_[static_cast<std::size_t>(cellIndex(cx, cy))].push_back(i);
        ++bucketInserts;
    }

    const auto t1 = std::chrono::steady_clock::now();

    const CycleCount workCycles = static_cast<CycleCount>(n) * 8 + bucketInserts * 6;
    metrics.wallMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
    metrics.workCycles += workCycles;
    metrics.totalCycles += workCycles;
}

void SequentialStageRunner::executeDensityPressureReal(StageMetrics& metrics, int stepIndex) {
    const auto t0 = std::chrono::steady_clock::now();

    const float h = state_.params.smoothingRadius;
    const float mass = state_.params.particleMass;
    const float restDensity = state_.params.restDensity;
    const float gasConstant = state_.params.gasConstant;
    const float densityFloor = restDensity * state_.params.minDensityRatio;
    const float maxPressure = state_.params.maxPressure;

    std::atomic<CycleCount> neighbourContributions{0};
    std::atomic<CycleCount> candidateReads{0};

    const int n = static_cast<int>(state_.particles.size());
    strategy_->runForRange(n, [&](int begin, int end, int) {
        for (int i = begin; i < end; ++i) {
            Particle& pi = state_.particles[static_cast<std::size_t>(i)];
            float density = 0.0f;

            int minX = 0;
            int maxX = 0;
            int minY = 0;
            int maxY = 0;
            neighbourCellBounds(pi, minX, maxX, minY, maxY);

            for (int cy = minY; cy <= maxY; ++cy) {
                for (int cx = minX; cx <= maxX; ++cx) {
                    const auto& bucket = gridBuckets_[static_cast<std::size_t>(cellIndex(cx, cy))];
                    for (int j : bucket) {
                        candidateReads.fetch_add(1, std::memory_order_relaxed);
                        const Particle& pj = state_.particles[static_cast<std::size_t>(j)];
                        const float dx = pi.position.x - pj.position.x;
                        const float dy = pi.position.y - pj.position.y;
                        const float r2 = dx * dx + dy * dy;

                        const float w = poly6Kernel(r2, h);
                        if (w > 0.0f) {
                            neighbourContributions.fetch_add(1, std::memory_order_relaxed);
                            density += mass * w;
                        }
                    }
                }
            }

            pi.density = std::max(density, densityFloor);
            const float unclampedPressure = gasConstant * (pi.density - restDensity);
            pi.pressure = std::clamp(unclampedPressure, -maxPressure, maxPressure);
        }
    });
    strategy_->barrier();

    const auto t1 = std::chrono::steady_clock::now();

    const CycleCount neighbourReads = neighbourContributions.load(std::memory_order_relaxed);
    const CycleCount candidateChecks = candidateReads.load(std::memory_order_relaxed);
    const CycleCount workCycles =
        candidateChecks * 4 + neighbourReads * 12 + static_cast<CycleCount>(n) * 14;
    CycleCount contextSwitchCycles = 0;
    CycleCount hiddenStallCycles = 0;
    CycleCount exposedStallCycles = 0;
    estimateStageStallBreakdown(
        StageId::DensityPressure,
        stepIndex,
        n,
        candidateChecks,
        neighbourReads,
        contextSwitchCycles,
        hiddenStallCycles,
        exposedStallCycles);

    metrics.wallMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
    metrics.workCycles += workCycles;
    metrics.idleCycles += contextSwitchCycles + exposedStallCycles;
    metrics.contextSwitchCycles += contextSwitchCycles;
    metrics.stallHiddenCycles += hiddenStallCycles;
    metrics.stallExposedCycles += exposedStallCycles;
    metrics.totalCycles += workCycles + contextSwitchCycles + exposedStallCycles;
}

void SequentialStageRunner::executeForcesReal(StageMetrics& metrics, int stepIndex) {
    const auto t0 = std::chrono::steady_clock::now();

    const float h = state_.params.smoothingRadius;
    const float h2 = h * h;
    const float mass = state_.params.particleMass;
    const float viscosity = state_.params.viscosity;
    const float gravity = state_.params.gravity;
    const float densityFloor = state_.params.restDensity * state_.params.minDensityRatio;

    std::atomic<CycleCount> neighbourInteractions{0};
    std::atomic<CycleCount> candidateReads{0};

    const int n = static_cast<int>(state_.particles.size());
    strategy_->runForRange(n, [&](int begin, int end, int) {
        for (int i = begin; i < end; ++i) {
            Particle& pi = state_.particles[static_cast<std::size_t>(i)];

            float forcePressureX = 0.0f;
            float forcePressureY = 0.0f;
            float forceViscosityX = 0.0f;
            float forceViscosityY = 0.0f;

            int minX = 0;
            int maxX = 0;
            int minY = 0;
            int maxY = 0;
            neighbourCellBounds(pi, minX, maxX, minY, maxY);

            for (int cy = minY; cy <= maxY; ++cy) {
                for (int cx = minX; cx <= maxX; ++cx) {
                    const auto& bucket = gridBuckets_[static_cast<std::size_t>(cellIndex(cx, cy))];
                    for (int j : bucket) {
                        candidateReads.fetch_add(1, std::memory_order_relaxed);
                        if (j == i) continue;

                        const Particle& pj = state_.particles[static_cast<std::size_t>(j)];
                        const float dx = pi.position.x - pj.position.x;
                        const float dy = pi.position.y - pj.position.y;
                        const float r2 = dx * dx + dy * dy;

                        if (r2 >= h2) continue;

                        const float r = std::sqrt(r2);
                        if (r <= 1e-6f) continue;

                        neighbourInteractions.fetch_add(1, std::memory_order_relaxed);

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
                }
            }

            pi.force.x = forcePressureX + forceViscosityX;
            pi.force.y = forcePressureY + forceViscosityY + gravity * pi.density;
        }
    });
    strategy_->barrier();

    const auto t1 = std::chrono::steady_clock::now();

    const CycleCount candidateChecks = candidateReads.load(std::memory_order_relaxed);
    const CycleCount interactions = neighbourInteractions.load(std::memory_order_relaxed);
    const CycleCount workCycles =
        candidateChecks * 5 + interactions * 26 + static_cast<CycleCount>(n) * 18;
    CycleCount contextSwitchCycles = 0;
    CycleCount hiddenStallCycles = 0;
    CycleCount exposedStallCycles = 0;
    estimateStageStallBreakdown(
        StageId::Forces,
        stepIndex,
        n,
        candidateChecks,
        interactions,
        contextSwitchCycles,
        hiddenStallCycles,
        exposedStallCycles);

    metrics.wallMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
    metrics.workCycles += workCycles;
    metrics.idleCycles += contextSwitchCycles + exposedStallCycles;
    metrics.contextSwitchCycles += contextSwitchCycles;
    metrics.stallHiddenCycles += hiddenStallCycles;
    metrics.stallExposedCycles += exposedStallCycles;
    metrics.totalCycles += workCycles + contextSwitchCycles + exposedStallCycles;
}

void SequentialStageRunner::executeIntegrateReal(StageMetrics& metrics) {
    const auto t0 = std::chrono::steady_clock::now();

    const float dt = config_.dt;
    const float densityFloor = state_.params.restDensity * state_.params.minDensityRatio;
    const float maxSpeed = state_.params.maxSpeed;

    const int n = static_cast<int>(state_.particles.size());
    for (int i = 0; i < n; ++i) {
        Particle& p = state_.particles[static_cast<std::size_t>(i)];

        const float safeDensity = std::max(p.density, densityFloor);
        const float ax = p.force.x / safeDensity;
        const float ay = p.force.y / safeDensity;

        p.velocity.x += ax * dt;
        p.velocity.y += ay * dt;

        const float speed2 = p.velocity.x * p.velocity.x + p.velocity.y * p.velocity.y;
        const float maxSpeed2 = maxSpeed * maxSpeed;
        if (speed2 > maxSpeed2) {
            const float speed = std::sqrt(speed2);
            const float scale = maxSpeed / speed;
            p.velocity.x *= scale;
            p.velocity.y *= scale;
        }

        p.position.x += p.velocity.x * dt;
        p.position.y += p.velocity.y * dt;
    }

    const auto t1 = std::chrono::steady_clock::now();

    const CycleCount workCycles = static_cast<CycleCount>(n) * 20;

    metrics.wallMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
    metrics.workCycles += workCycles;
    metrics.totalCycles += workCycles;
}

void SequentialStageRunner::executeBoundaryReal(StageMetrics& metrics) {
    const auto t0 = std::chrono::steady_clock::now();

    const float minX = 0.0f;
    const float minY = 0.0f;
    const float maxX = state_.params.domainWidth;
    const float maxY = state_.params.domainHeight;
    const float damping = state_.params.boundaryDamping;

    CycleCount collisions = 0;

    const int n = static_cast<int>(state_.particles.size());
    for (int i = 0; i < n; ++i) {
        Particle& p = state_.particles[static_cast<std::size_t>(i)];

        if (p.position.x < minX) {
            p.position.x = minX;
            p.velocity.x *= damping;
            ++collisions;
        } else if (p.position.x > maxX) {
            p.position.x = maxX;
            p.velocity.x *= damping;
            ++collisions;
        }

        if (p.position.y < minY) {
            p.position.y = minY;
            p.velocity.y *= damping;
            ++collisions;
        } else if (p.position.y > maxY) {
            p.position.y = maxY;
            p.velocity.y *= damping;
            ++collisions;
        }
    }

    const auto t1 = std::chrono::steady_clock::now();

    const CycleCount workCycles = static_cast<CycleCount>(n) * 8 + collisions * 10;

    metrics.wallMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
    metrics.workCycles += workCycles;
    metrics.totalCycles += workCycles;
}

void SequentialStageRunner::executeMetricsReal(StageMetrics& metrics, int stepIndex) {
    const auto t0 = std::chrono::steady_clock::now();

    const int n = static_cast<int>(state_.particles.size());
    const float mass = state_.params.particleMass;

    double densitySum = 0.0;
    double speedSum = 0.0;
    double kineticSum = 0.0;
    double maxSpeedObserved = 0.0;
    int invalidParticleCount = 0;

    for (int i = 0; i < n; ++i) {
        const Particle& p = state_.particles[static_cast<std::size_t>(i)];
        const double vx = static_cast<double>(p.velocity.x);
        const double vy = static_cast<double>(p.velocity.y);
        const double speed = std::sqrt(vx * vx + vy * vy);
        const double speed2 = vx * vx + vy * vy;
        const bool invalid = !std::isfinite(vx) || !std::isfinite(vy) ||
                             !std::isfinite(static_cast<double>(p.density)) ||
                             !std::isfinite(static_cast<double>(p.pressure));
        if (invalid) {
            ++invalidParticleCount;
        }

        densitySum += static_cast<double>(p.density);
        speedSum += speed;
        kineticSum += 0.5 * static_cast<double>(mass) * speed2;
        maxSpeedObserved = std::max(maxSpeedObserved, speed);
    }

    StepMetricsSnapshot snapshot;
    snapshot.stepIndex = stepIndex;
    snapshot.avgDensity = (n > 0) ? densitySum / static_cast<double>(n) : 0.0;
    snapshot.avgSpeed = (n > 0) ? speedSum / static_cast<double>(n) : 0.0;
    snapshot.totalKineticEnergy = kineticSum;
    snapshot.maxSpeedObserved = maxSpeedObserved;
    snapshot.invalidParticleCount = invalidParticleCount;

    if (stepIndex >= 0 && stepIndex < static_cast<int>(stepSnapshots_.size())) {
        stepSnapshots_[static_cast<std::size_t>(stepIndex)] = snapshot;
    } else {
        stepSnapshots_.push_back(snapshot);
    }

    const auto t1 = std::chrono::steady_clock::now();

    const CycleCount workCycles = static_cast<CycleCount>(n) * 14;
    metrics.wallMs += std::chrono::duration<double, std::milli>(t1 - t0).count();
    metrics.workCycles += workCycles;
    metrics.totalCycles += workCycles;
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

int SequentialStageRunner::activeWorkersForCycles(int itemCount) const {
    const int requestedWorkers = std::max(1, config_.threads);
    return std::max(1, std::min(requestedWorkers, itemCount));
}

void SequentialStageRunner::estimateStageStallBreakdown(
    StageId stage,
    int stepIndex,
    int itemCount,
    CycleCount candidateChecks,
    CycleCount interactions,
    CycleCount& contextSwitchCycles,
    CycleCount& hiddenStallCycles,
    CycleCount& exposedStallCycles) const {
    contextSwitchCycles = 0;
    hiddenStallCycles = 0;
    exposedStallCycles = 0;

    if (stage != StageId::DensityPressure && stage != StageId::Forces) {
        return;
    }

    const int workers = activeWorkersForCycles(itemCount);
    const bool isSequential = dynamic_cast<const th::SequentialThreadStrategy*>(strategy_.get()) != nullptr;
    const bool isChunked = dynamic_cast<const th::ParallelChunkedThreadStrategy*>(strategy_.get()) != nullptr;
    const auto* fgmt = dynamic_cast<const th::FGMTRoundRobinStrategy*>(strategy_.get());
    const bool isSmt = dynamic_cast<const th::SMTThreadStrategy*>(strategy_.get()) != nullptr;
    const bool isCmp = dynamic_cast<const th::CMPThreadStrategy*>(strategy_.get()) != nullptr;

    // Calibration knobs (v1): tuned to bring simulated cycle trends closer to observed
    // real-machine behavior under thread contention. Keep these explicit for report traceability.
    constexpr CycleCount kFgContextPerQuantum = 16ULL;
    constexpr CycleCount kFgContextPerWorker = 18ULL;
    constexpr CycleCount kCgContextPerWorker = 40ULL;
    constexpr CycleCount kCgContextStepJitter = 20ULL;
    constexpr CycleCount kSmtContextPerWorker = 54ULL;
    constexpr CycleCount kCmpContextPerWorker = 62ULL;
    constexpr CycleCount kSmtBasePenalty = 140ULL;
    constexpr CycleCount kCmpBasePenalty = 175ULL;

    const CycleCount stageBase = (stage == StageId::DensityPressure) ? 120ULL : 180ULL;
    const CycleCount memoryStalls = candidateChecks * ((stage == StageId::DensityPressure) ? 2ULL : 3ULL);
    const CycleCount interactionStalls = interactions * ((stage == StageId::DensityPressure) ? 1ULL : 2ULL);
    const CycleCount jitter = static_cast<CycleCount>((config_.seed + 17 * stepIndex + static_cast<int>(stage)) % 7);
    CycleCount rawStallCycles = stageBase + memoryStalls + interactionStalls + jitter * 15ULL;

    // Apply a contention-oriented penalty for real-thread strategies.
    if (isSmt) {
        rawStallCycles += kSmtBasePenalty + static_cast<CycleCount>(workers) * 14ULL;
    } else if (isCmp) {
        rawStallCycles += kCmpBasePenalty + static_cast<CycleCount>(workers) * 18ULL;
    }

    if (isSequential) {
        exposedStallCycles = rawStallCycles;
        return;
    }

    if (fgmt != nullptr) {
        const CycleCount workerTerm = static_cast<CycleCount>(std::max(0, workers - 1));
        const CycleCount hiddenPercent = std::min<CycleCount>(80ULL, 35ULL + workerTerm * 12ULL);
        hiddenStallCycles = (rawStallCycles * hiddenPercent) / 100ULL;
        exposedStallCycles = rawStallCycles - hiddenStallCycles;

        const int quantum = std::max(1, fgmt->quantumItems());
        const CycleCount quanta = static_cast<CycleCount>((itemCount + quantum - 1) / quantum);
        contextSwitchCycles = quanta * kFgContextPerQuantum + static_cast<CycleCount>(workers) * kFgContextPerWorker;
        return;
    }

    if (isChunked) {
        const CycleCount workerTerm = static_cast<CycleCount>(std::max(0, workers - 1));
        const CycleCount hiddenPercent = std::min<CycleCount>(60ULL, 15ULL + workerTerm * 10ULL);
        hiddenStallCycles = (rawStallCycles * hiddenPercent) / 100ULL;
        exposedStallCycles = rawStallCycles - hiddenStallCycles;

        contextSwitchCycles = static_cast<CycleCount>(workers) * kCgContextPerWorker +
                              static_cast<CycleCount>((stepIndex % 3) + 1) * kCgContextStepJitter;
        return;
    }

    if (isSmt) {
        const CycleCount workerTerm = static_cast<CycleCount>(std::max(0, workers - 1));
        const CycleCount hiddenPercent = std::min<CycleCount>(55ULL, 20ULL + workerTerm * 8ULL);
        hiddenStallCycles = (rawStallCycles * hiddenPercent) / 100ULL;
        exposedStallCycles = rawStallCycles - hiddenStallCycles;
        contextSwitchCycles = static_cast<CycleCount>(workers) * kSmtContextPerWorker +
                              static_cast<CycleCount>((stepIndex % 4) + 1) * 12ULL;
        return;
    }

    if (isCmp) {
        const CycleCount workerTerm = static_cast<CycleCount>(std::max(0, workers - 1));
        const CycleCount hiddenPercent = std::min<CycleCount>(42ULL, 12ULL + workerTerm * 6ULL);
        hiddenStallCycles = (rawStallCycles * hiddenPercent) / 100ULL;
        exposedStallCycles = rawStallCycles - hiddenStallCycles;
        contextSwitchCycles = static_cast<CycleCount>(workers) * kCmpContextPerWorker +
                              static_cast<CycleCount>((stepIndex % 5) + 1) * 16ULL;
        return;
    }

    exposedStallCycles = rawStallCycles;
}

int SequentialStageRunner::clampCellX(int cellX) const {
    if (cellX < 0) return 0;
    if (cellX >= gridWidth_) return gridWidth_ - 1;
    return cellX;
}

int SequentialStageRunner::clampCellY(int cellY) const {
    if (cellY < 0) return 0;
    if (cellY >= gridHeight_) return gridHeight_ - 1;
    return cellY;
}

int SequentialStageRunner::cellIndex(int cellX, int cellY) const {
    return cellY * gridWidth_ + cellX;
}

void SequentialStageRunner::neighbourCellBounds(
    const Particle& particle,
    int& minX,
    int& maxX,
    int& minY,
    int& maxY) const {
    int cx = static_cast<int>(std::floor(particle.position.x / gridCellSize_));
    int cy = static_cast<int>(std::floor(particle.position.y / gridCellSize_));
    cx = clampCellX(cx);
    cy = clampCellY(cy);

    minX = clampCellX(cx - 1);
    maxX = clampCellX(cx + 1);
    minY = clampCellY(cy - 1);
    maxY = clampCellY(cy + 1);
}

} // namespace sim
