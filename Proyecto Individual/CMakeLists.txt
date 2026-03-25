cmake_minimum_required(VERSION 3.16)
project(SPH_Lab_Restart LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(lab_runner
    src/main.cpp
    src/simulation/SequentialStageRunner.cpp
)

target_include_directories(lab_runner PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
