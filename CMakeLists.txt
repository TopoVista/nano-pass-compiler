cmake_minimum_required(VERSION 3.20)

project(NanoPassCompilerLab LANGUAGES C CXX)

# Enable CTest at the root so `ctest --test-dir build` discovers all tests.
enable_testing()

option(NANO_BUILD_TESTS        "Build NanoPass tests"                    ON)
option(NANO_BUILD_JIT          "Build optional ORC JIT support"          OFF)
option(NANO_BUILD_WEB_SERVER   "Build the optional JSON API server"      OFF)
option(NANO_ENABLE_SANITIZERS  "Enable address and undefined-behavior sanitizers" OFF)
option(NANO_ENABLE_FUZZING     "Build fuzzing targets"                   OFF)
option(NANO_ENABLE_ALIVE2      "Enable optional Alive2 integration"      OFF)

add_subdirectory(compiler)
