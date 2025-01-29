cmake_minimum_required(VERSION 3.16.3)

# Find or fetch Google Test
include(FetchContent)

FetchContent_Declare(
    GoogleTest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG b514bdc898e2951020cbdca1304b75f5950d1f59 # v1.15.2
)

FetchContent_MakeAvailable(GoogleTest)
