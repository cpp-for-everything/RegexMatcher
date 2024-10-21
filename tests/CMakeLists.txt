# Add the test executable
add_executable(playground
   playground.cpp
)

# Link Google Test and the library under test
target_link_libraries(playground PRIVATE
   RegexMatcher::core
)

include(CTest)
enable_testing()

# Download and include Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
    DOWNLOAD_EXTRACT_TIMESTAMP true
)
FetchContent_MakeAvailable(googletest)

if (CMAKE_C_COMPILER_ID MATCHES "(Apple)?[Cc]lang" OR CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?[Cc]lang"
    OR CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
   # Includes code coverage CMake tool
   list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake_tools)
   include(code-coverage)
   add_code_coverage() # Adds instrumentation to all targets
   add_code_coverage_all_targets(EXCLUDE ${CMAKE_SOURCE_DIR}/tests/*)
endif()

# Register the test with CTest
include(GoogleTest)

add_executable(all_tests
   main.cpp
)

# Link Google Test and the library under test
target_link_libraries(all_tests PRIVATE
   gtest_main
   RegexMatcher::core
)

gtest_discover_tests(all_tests)

if (CMAKE_C_COMPILER_ID MATCHES "(Apple)?[Cc]lang" OR CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?[Cc]lang"
      OR CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
   target_code_coverage(all_tests AUTO ALL EXCLUDE tests/*)
endif()