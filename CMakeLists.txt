cmake_minimum_required(VERSION 3.16.3)

project(RegexMatcher VERSION 1.0.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/install" CACHE PATH "" FORCE)
endif()

configure_file(RegexMatcherConfig.h.in ${CMAKE_BINARY_DIR}/config/RegexMatcherConfig.h)
add_subdirectory(core)
add_subdirectory(tests)