cmake_minimum_required(VERSION 3.16.3)

project(RegexMatcher VERSION 1.0.0.0 LANGUAGES CXX)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Debug flag
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDEBUG)
endif()

# Add subdirectories
add_subdirectory(external)
add_subdirectory(include)
add_subdirectory(tests)

# Install and export the targets.
install(TARGETS RegexMatcher
  DESTINATION lib/RegexMatcher-${PROJECT_VERSION}
  EXPORT RegexMatcher-targets
  FILE_SET regex_matcher_headers
)
install(EXPORT RegexMatcher-targets
  DESTINATION lib/RegexMatcher-${PROJECT_VERSION})

# Create and install package configuration and version files.
configure_file(
  ${RegexMatcher_SOURCE_DIR}/pkg/RegexMatcher-config.cmake.in
  ${RegexMatcher_BINARY_DIR}/pkg/RegexMatcher-config.cmake @ONLY)

configure_file(
  ${RegexMatcher_SOURCE_DIR}/RegexMatcher-config-version.cmake.in
  ${RegexMatcher_BINARY_DIR}/RegexMatcher-config-version.cmake @ONLY)

install(FILES ${RegexMatcher_BINARY_DIR}/pkg/RegexMatcher-config.cmake
  ${RegexMatcher_BINARY_DIR}/RegexMatcher-config-version.cmake
  DESTINATION lib/RegexMatcher-${PROJECT_VERSION})

# Make project usable from build tree.
export(TARGETS RegexMatcher FILE RegexMatcher-targets.cmake)
configure_file(${RegexMatcher_SOURCE_DIR}/RegexMatcher-config.cmake.in
  ${RegexMatcher_BINARY_DIR}/RegexMatcher-config.cmake @ONLY)