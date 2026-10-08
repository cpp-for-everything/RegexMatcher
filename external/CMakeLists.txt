# Test and benchmark frameworks, fetched only when tests or benchmarks are built.
# A local copy can replace a download: -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=<dir>,
# -DFETCHCONTENT_SOURCE_DIR_GOOGLEBENCHMARK=<dir>.
include(FetchContent)

if(REGEXMATCHER_BUILD_TESTS)
    FetchContent_Declare(
        GoogleTest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG b514bdc898e2951020cbdca1304b75f5950d1f59 # v1.15.2
    )
    # Prevent GoogleTest from overriding standard CMake variables
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(GoogleTest)
endif()

if(REGEXMATCHER_BUILD_BENCHMARKS)
    FetchContent_Declare(
        GoogleBenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    # Disable benchmark tests to speed up build
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(GoogleBenchmark)
endif()
