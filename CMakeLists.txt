cmake_minimum_required(VERSION 3.16.3)

project(RegexMatcher VERSION 1.0.0.0 LANGUAGES CXX)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Define installation prefix
if(NOT CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install" CACHE PATH "" FORCE)
endif()

# Debug flag
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDEBUG)
endif()

# Config header
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/RegexMatcherConfig.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/matcher/RegexMatcherConfig.h)

# Add subdirectories
add_subdirectory(include)
add_subdirectory(tests)

# Package configuration
include(CMakePackageConfigHelpers)

set(CONFIG_INSTALL_DIR "lib/cmake/${PROJECT_NAME}")

configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/RegexMatcherConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/RegexMatcherConfig.cmake
  INSTALL_DESTINATION ${CONFIG_INSTALL_DIR}
)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/RegexMatcherConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(
  FILES
  ${CMAKE_CURRENT_BINARY_DIR}/RegexMatcherConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/RegexMatcherConfigVersion.cmake
  DESTINATION ${CONFIG_INSTALL_DIR}
)

# Export targets
install(
  EXPORT RegexMatcherTargets
  FILE RegexMatcherTargets.cmake
  NAMESPACE RegexMatcher::
  DESTINATION ${CONFIG_INSTALL_DIR}
)
