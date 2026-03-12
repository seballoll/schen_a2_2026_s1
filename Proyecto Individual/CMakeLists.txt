cmake_minimum_required(VERSION 3.16)
project(SPH_FluidSimulation LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find required packages
find_package(SFML 2.5 COMPONENTS graphics window system QUIET)
find_package(Threads REQUIRED)

# If SFML not found via config, try pkg-config
if(NOT SFML_FOUND)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(SFML sfml-graphics sfml-window sfml-system)
    endif()
endif()

# ============================================================
# Source files
# ============================================================
set(CORE_SOURCES
    src/core/Particle.cpp
    src/core/SPHSolver.cpp
    src/core/SpatialHash.cpp
    src/core/Kernels.cpp
)

set(THREADING_SOURCES
    src/threading/SequentialSolver.cpp
    src/threading/FineGrainedSolver.cpp
    src/threading/CoarseGrainedSolver.cpp
    src/threading/SMTSolver.cpp
    src/threading/CMPSolver.cpp
)

set(METRICS_SOURCES
    src/metrics/PerformanceMetrics.cpp
)

# ============================================================
# Main simulation with visualization
# ============================================================
if(SFML_FOUND)
    add_executable(sph_simulation
        src/main.cpp
        ${CORE_SOURCES}
        ${THREADING_SOURCES}
        ${METRICS_SOURCES}
        src/visualization/Renderer.cpp
    )
    target_include_directories(sph_simulation PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${SFML_INCLUDE_DIRS}
    )
    target_link_libraries(sph_simulation PRIVATE
        sfml-graphics sfml-window sfml-system
        Threads::Threads
    )
    message(STATUS "SFML found — building with visualization")
else()
    message(WARNING "SFML not found — building WITHOUT visualization (headless only)")
    add_executable(sph_simulation
        src/main_headless.cpp
        ${CORE_SOURCES}
        ${THREADING_SOURCES}
        ${METRICS_SOURCES}
    )
    target_include_directories(sph_simulation PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(sph_simulation PRIVATE
        Threads::Threads
    )
endif()

# ============================================================
# Visual comparison executable (side-by-side models)
# ============================================================
if(SFML_FOUND)
    add_executable(sph_compare
        src/visual_compare.cpp
        ${CORE_SOURCES}
        ${THREADING_SOURCES}
        ${METRICS_SOURCES}
        src/visualization/Renderer.cpp
    )
    target_include_directories(sph_compare PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${SFML_INCLUDE_DIRS}
    )
    target_link_libraries(sph_compare PRIVATE
        sfml-graphics sfml-window sfml-system
        Threads::Threads
    )
    message(STATUS "Building sph_compare — visual side-by-side comparison")
endif()

# ============================================================
# Benchmark executable (always headless)
# ============================================================
add_executable(sph_benchmark
    src/benchmark.cpp
    ${CORE_SOURCES}
    ${THREADING_SOURCES}
    ${METRICS_SOURCES}
)
target_include_directories(sph_benchmark PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(sph_benchmark PRIVATE
    Threads::Threads
)
