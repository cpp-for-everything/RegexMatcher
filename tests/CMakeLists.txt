# Enable testing
enable_testing()

# Find or fetch Google Test
include(FetchContent)

FetchContent_Declare(
   gtest
   URL https://github.com/google/googletest/archive/refs/tags/release-1.12.1.zip
)

if(CMAKE_C_COMPILER_ID MATCHES "(Apple)?[Cc]lang" OR CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?[Cc]lang"
   OR CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
   # Includes code coverage CMake tool
   list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake_tools)
   include(code-coverage)
   add_code_coverage() # Adds instrumentation to all targets
   add_code_coverage_all_targets(EXCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/../tests/*)
endif()

# Prevent GoogleTest from overriding standard CMake variables
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(gtest)

# Add test executable
add_executable(tests main.cpp)
target_include_directories(
   tests
   PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(tests
   PRIVATE
   RegexMatcher
   gtest
   gtest_main
)

# Add tests
include(GoogleTest)
gtest_discover_tests(tests)

if(CMAKE_C_COMPILER_ID MATCHES "(Apple)?[Cc]lang" OR CMAKE_CXX_COMPILER_ID MATCHES "(Apple)?[Cc]lang"
   OR CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
   target_code_coverage(tests AUTO ALL EXCLUDE tests/*)
endif()

# Optional: Install test executable
install(
   TARGETS tests
   RUNTIME DESTINATION bin
)
