cmake_minimum_required(VERSION 3.25)

project(RegexMatcher VERSION 2.1.0.0 LANGUAGES CXX)

# Two header-only components:
#   RegexMatcher::core   the multi-pattern regex engine (include/matcher/core.hpp), C++17
#   RegexMatcher::route  the HTTP route matcher (include/matcher/route.hpp), C++23
# Each target states its own language standard, so a project that uses only the engine
# keeps C++17.

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(REGEXMATCHER_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
option(REGEXMATCHER_BUILD_BENCHMARKS "Build the regex engine's benchmarks (fetches Google Benchmark)" OFF)
set(REGEXMATCHER_SANITIZER "" CACHE STRING
    "Sanitizer for every target of this build: address+undefined, thread or memory (clang, gcc); address (MSVC, clang-cl)")
set(REGEXMATCHER_MSAN_LIBCXX "" CACHE PATH "Prefix of a MemorySanitizer-instrumented libc++ (REGEXMATCHER_SANITIZER=memory)")

# Debug flag
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDEBUG)
endif()

# Sanitizers apply to every target of the build, fetched test frameworks included, so that
# MemorySanitizer sees instrumented code only.
if(REGEXMATCHER_SANITIZER)
  if(MSVC)
    if(NOT REGEXMATCHER_SANITIZER STREQUAL "address")
      message(FATAL_ERROR "REGEXMATCHER_SANITIZER=${REGEXMATCHER_SANITIZER}: MSVC and clang-cl support address only")
    endif()
    add_compile_options(/fsanitize=address /Zi)
    add_link_options(/INCREMENTAL:NO)
  else()
    string(REPLACE "+" "," _rm_san "${REGEXMATCHER_SANITIZER}")
    add_compile_options(-fsanitize=${_rm_san} -fno-omit-frame-pointer -fno-sanitize-recover=all -g)
    add_link_options(-fsanitize=${_rm_san})
    if(_rm_san MATCHES "memory")
      if(NOT EXISTS "${REGEXMATCHER_MSAN_LIBCXX}/include/c++/v1")
        message(FATAL_ERROR "REGEXMATCHER_SANITIZER=memory needs REGEXMATCHER_MSAN_LIBCXX with include/c++/v1")
      endif()
      add_compile_options(-fsanitize-memory-track-origins=2
        "$<$<COMPILE_LANGUAGE:CXX>:-stdlib=libc++>"
        "$<$<COMPILE_LANGUAGE:CXX>:-isystem${REGEXMATCHER_MSAN_LIBCXX}/include/c++/v1>")
      add_link_options(-stdlib=libc++ "-L${REGEXMATCHER_MSAN_LIBCXX}/lib"
        "-Wl,-rpath,${REGEXMATCHER_MSAN_LIBCXX}/lib" -lc++abi)
    endif()
  endif()
  message(STATUS "RegexMatcher: sanitizer ${REGEXMATCHER_SANITIZER}")
endif()

add_subdirectory(include)

if(REGEXMATCHER_BUILD_TESTS OR REGEXMATCHER_BUILD_BENCHMARKS)
  add_subdirectory(external)
endif()
if(REGEXMATCHER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Install and export the targets.
# The headers install to include/matcher, the CMake files to lib/RegexMatcher-<version>.
install(TARGETS RegexMatcher RegexMatcherRoute
  EXPORT RegexMatcher-targets
  DESTINATION lib/RegexMatcher-${PROJECT_VERSION}
  FILE_SET regex_matcher_headers DESTINATION include
  FILE_SET route_headers DESTINATION include
)
install(EXPORT RegexMatcher-targets
  NAMESPACE RegexMatcher::
  DESTINATION lib/RegexMatcher-${PROJECT_VERSION})

# Create and install package configuration and version files. find_package looks for
# <PackageName>Config.cmake or <lowercase package name>-config.cmake, so the files carry the
# lowercase name: RegexMatcher-config.cmake is found only where file names ignore case.
configure_file(
  ${RegexMatcher_SOURCE_DIR}/pkg/RegexMatcher-config.cmake.in
  ${RegexMatcher_BINARY_DIR}/pkg/regexmatcher-config.cmake @ONLY)

configure_file(
  ${RegexMatcher_SOURCE_DIR}/RegexMatcher-config-version.cmake.in
  ${RegexMatcher_BINARY_DIR}/regexmatcher-config-version.cmake @ONLY)

install(FILES ${RegexMatcher_BINARY_DIR}/pkg/regexmatcher-config.cmake
  ${RegexMatcher_BINARY_DIR}/regexmatcher-config-version.cmake
  DESTINATION lib/RegexMatcher-${PROJECT_VERSION})

# Make project usable from build tree.
export(TARGETS RegexMatcher RegexMatcherRoute NAMESPACE RegexMatcher:: FILE RegexMatcher-targets.cmake)
configure_file(${RegexMatcher_SOURCE_DIR}/RegexMatcher-config.cmake.in
  ${RegexMatcher_BINARY_DIR}/regexmatcher-config.cmake @ONLY)
