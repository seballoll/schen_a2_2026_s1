#pragma once

#include <cstdint>

// ============================================================
// SimStats — simulated hardware counters for the project "lab"
//
// These counters are a *model* (not perf/PMU).
// - totalCycles: total simulated cycles elapsed
// - workCycles: cycles executing useful work
// - idleCycles: cycles with no runnable HW thread (exposed stalls)
// - contextSwitchCycles: scheduler overhead
// - stallCycles*: stall latency bookkeeping (hidden vs exposed)
// ============================================================

enum class StallKind : uint8_t {
    L1 = 0,
    L2 = 1,
    L3 = 2,
    Memory = 3,
    Sync = 4,
};

struct SimStats {
    uint64_t totalCycles = 0;
    uint64_t workCycles = 0;
    uint64_t idleCycles = 0;
    uint64_t contextSwitchCycles = 0;

    uint64_t stallEvents = 0;
    uint64_t stallCyclesTotal = 0;
    uint64_t stallCyclesHidden = 0;
    uint64_t stallCyclesExposed = 0;

    uint64_t stallEventsL1 = 0;
    uint64_t stallEventsL2 = 0;
    uint64_t stallEventsL3 = 0;
    uint64_t stallEventsMem = 0;
    uint64_t stallEventsSync = 0;

    void reset() { *this = SimStats{}; }
};
