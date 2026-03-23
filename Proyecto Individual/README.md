# SPH Fluid Simulation — Multithreading Execution Models

## CE4302 — Computer Architecture II — Individual Project

2D Smoothed Particle Hydrodynamics (SPH) simulation comparing different thread-level parallelism strategies.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    SPH Pipeline (per timestep)           │
│                                                         │
│  1. Build Spatial Hash (neighbour structure)             │
│  2. Compute Density & Pressure   ← parallelisable       │
│  3. Compute Forces               ← parallelisable       │
│        ↑ BARRIER (step 2 must finish before step 3)     │
│  4. Integrate (update vel/pos)   ← parallelisable       │
│  5. Enforce Boundary conditions  ← parallelisable       │
└─────────────────────────────────────────────────────────┘
```

## Multithreading Models

| Model | Description | Synchronisation |
|-------|-------------|-----------------|
| **Sequential** | Single-thread baseline | None |
| **Fine-Grained (FGMT)** | Spawn threads per-stage, join after each | Implicit barrier per stage (thread join) |
| **Coarse-Grained (CGMT)** | Persistent threads walk all stages, `std::barrier` between stages | `std::barrier` (C++20) |
| **SMT (HyperThreading)** | Threads pinned to logical cores sharing a physical core | Same as FGMT + CPU affinity |
| **CMP (Chip Multiprocessing)** | Threads pinned to separate physical cores | Same as FGMT + CPU affinity |

## Metrics

- **Execution time** (wall-clock seconds)
- **Speedup** S(p) = T_sequential / T_parallel
- **Parallel efficiency** E(p) = S(p) / p
- **Scalability** — measured across {2, 4, 8, …, N} threads

## Key SPH Variables

| Variable | Symbol | Config key | Description |
|----------|--------|------------|-------------|
| Smoothing radius | h | `SMOOTHING_RADIUS` | Kernel support radius |
| Rest density | ρ₀ | `REST_DENSITY` | Target density (1000 for water) |
| Gas constant | k | `GAS_CONSTANT` | Stiffness in EOS |
| Viscosity | μ | `VISCOSITY` | Dynamic viscosity coefficient |
| Particle mass | m | `PARTICLE_MASS` | Mass of each particle |
| Timestep | Δt | `DT` | Integration timestep |
| Gravity | g | `GRAVITY` | Gravitational acceleration |

## Building

### Prerequisites

- C++20 compiler (GCC ≥ 10 or Clang ≥ 14)
- CMake ≥ 3.16
- SFML ≥ 2.5 (for visualisation; optional)
- pthreads (Linux)

### Install SFML (Ubuntu/Debian)

```bash
sudo apt-get install libsfml-dev
```

### Build

```bash
cd "Proyecto Individual"
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Run

There are **3 executables** — pick the one you need:

#### 1. `sph_simulation` — Run a single model with visualization

```bash
./sph_simulation [model] [threads] [particles] [dt]
```

| Model arg | Description |
|-----------|-------------|
| `seq`     | Sequential (single-threaded baseline) |
| `fgmt`    | Fine-Grained Multithreading |
| `cgmt`    | Coarse-Grained Multithreading |
| `smt`     | SMT / HyperThreading (pin to logical cores) |
| `cmp`     | CMP / Chip Multiprocessing (pin to physical cores) |

**Examples — run each model individually:**

```bash
# Sequential baseline (1 thread)
./sph_simulation seq

# Fine-Grained MT with 4 threads
./sph_simulation fgmt 4

# Fine-Grained MT with 4 threads, 5000 particles, dt=0.002
./sph_simulation fgmt 4 5000 0.002

# Coarse-Grained MT with 8 threads
./sph_simulation cgmt 8

# SMT / HyperThreading with 4 threads
./sph_simulation smt 4

# CMP / Chip Multiprocessing with 4 threads
./sph_simulation cmp 4

# Show help
./sph_simulation --help
```

Each window shows a **HUD overlay** with: model name, thread count, step number, ms/step, and particle count.

#### 2. `sph_compare` — Visual side-by-side comparison

See multiple models running **simultaneously in a split-screen** window — same initial conditions, same timestep, so you can directly compare behavior and speed.

```bash
./sph_compare [threads] [particles] [dt] [model1 model2 ...]
```

**Examples:**

```bash
# All 5 models at 4 threads (2×3 grid)
./sph_compare 4

# Compare sequential vs FGMT vs CMP
./sph_compare 4 2000 0.002 seq fgmt cmp

# Compare just FGMT vs CGMT at 8 threads
./sph_compare 8 2000 0.002 fgmt cgmt

# All parallel models at 4 threads (no sequential)
./sph_compare 4 2000 0.002 fgmt cgmt smt cmp
```

Each panel shows its model label and live ms/step timing.

#### 3. `sph_benchmark` — Headless performance benchmark

Runs all models at multiple thread counts, prints a comparison table, and exports to CSV. No window needed.

```bash
./sph_benchmark [numSteps] [numParticles] [dt]
```

**Examples:**

```bash
# Default: 200 steps, 2000 particles
./sph_benchmark

# Quick test: 50 steps
./sph_benchmark 50

# Longer run with more particles
./sph_benchmark 500 5000

# Longer run, more particles, smaller dt
./sph_benchmark 500 5000 0.001
```

Output: formatted table with execution time, speedup, efficiency for every model × thread count, plus `benchmark_results.csv`.

#### 4. `sph_dummy` — Dummy/partial run per model

Runs **one** measurement per model (Sequential, FGMT, CGMT, SMT, CMP) using the **same** thread count for parallel models.
You can choose a **partial pipeline stage** to keep the run fast for presentations.

```bash
./sph_dummy [threads] [steps] [particles] [dt] [stage]
```

Stages:
- `neighbours` — only build neighbour structure
- `density` — neighbours + density/pressure
- `forces` — neighbours + density/pressure + forces (default)
- `integrate` — up to integration
- `boundary` / `full` — full timestep pipeline

**Examples:**

```bash
# Fast “partial” demo (up to forces) at 4 threads
./sph_dummy 4 100 2000 0.002 forces

# Even faster: only density stage
./sph_dummy 4 100 2000 0.002 density
```

## Project Structure

```
Proyecto Individual/
├── CMakeLists.txt
├── include/
│   ├── Config.h               # All tunable parameters
│   ├── Vec2.h                 # 2D vector math
│   ├── core/
│   │   ├── Particle.h         # Particle data structure
│   │   ├── Kernels.h          # SPH smoothing kernels
│   │   ├── SpatialHash.h      # Spatial hashing for neighbours
│   │   └── SPHSolver.h        # Base solver (sequential pipeline)
│   ├── threading/
│   │   ├── SequentialSolver.h  # Baseline
│   │   ├── FineGrainedSolver.h # FGMT
│   │   ├── CoarseGrainedSolver.h # CGMT
│   │   ├── SMTSolver.h        # SMT / HyperThreading
│   │   └── CMPSolver.h        # CMP
│   ├── metrics/
│   │   └── PerformanceMetrics.h
│   └── visualization/
│       └── Renderer.h          # SFML renderer
└── src/
    ├── main.cpp                # Visual simulation entry
    ├── main_headless.cpp       # Headless entry (no SFML)
    ├── benchmark.cpp           # Full comparison benchmark
    ├── visual_compare.cpp      # Side-by-side visual comparison
    ├── core/                   # Implementation files
    ├── threading/              # Threading model implementations
    ├── metrics/
    └── visualization/
```
