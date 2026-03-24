#pragma once

#include "Config.h"
#include "metrics/SimStats.h"

#include <cstdint>
#include <utility>

namespace SimUtil {

static inline uint64_t mix64(uint64_t x) {
    x ^= x >> 33;
    x *= 0xff51afd7ed558ccdULL;
    x ^= x >> 33;
    x *= 0xc4ceb9fe1a85ec53ULL;
    x ^= x >> 33;
    return x;
}

static inline double u01(uint64_t x) {
    // Use top 53 bits for uniform double in [0,1)
    return (x >> 11) * (1.0 / 9007199254740992.0);
}

static inline uint64_t makeKey(uint64_t seed, uint64_t step, int stage, int particleIndex, int threadId = 0) {
    uint64_t x = seed;
    x ^= mix64(step + 0x9e3779b97f4a7c15ULL);
    x ^= mix64(static_cast<uint64_t>(stage) * 0xD1B54A32D192ED03ULL);
    x ^= mix64(static_cast<uint64_t>(particleIndex) * 0x94D049BB133111EBULL);
    x ^= mix64(static_cast<uint64_t>(threadId) * 0xBF58476D1CE4E5B9ULL);
    return mix64(x);
}

// Returns {kind, latencyCycles}. latency=0 => no stall.
static inline std::pair<StallKind, uint64_t> pickStall(uint64_t seed, uint64_t step, int stage, int particleIndex, int threadId = 0) {
    double r = u01(makeKey(seed, step, stage, particleIndex, threadId));

    if (r < Config::PROB_MEM_STALL) {
        return {StallKind::Memory, Config::STALL_MEM_CYCLES};
    }
    if (r < Config::PROB_MEM_STALL + Config::PROB_L3_MISS) {
        return {StallKind::L3, Config::STALL_L3_CYCLES};
    }
    if (r < Config::PROB_MEM_STALL + Config::PROB_L3_MISS + Config::PROB_L2_MISS) {
        return {StallKind::L2, Config::STALL_L2_CYCLES};
    }

    return {StallKind::L1, 0};
}

} // namespace SimUtil
