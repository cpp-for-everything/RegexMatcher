cmake_minimum_required(VERSION 3.16.3)

project(RegexMatcher VERSION 1.0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DDEBUG)
endif()

if(NOT DEFINED CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT OR CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/install" CACHE PATH "" FORCE)
endif()

configure_file(RegexMatcherConfig.h.in ${CMAKE_BINARY_DIR}/config/RegexMatcherConfig.h)
add_subdirectory(core)

include(CTest)
enable_testing()
add_subdirectory(tests)

set(GIT_SHA
    "Unknown"
    CACHE STRING "SHA this build was generated from")
string(
  SUBSTRING "${GIT_SHA}"
            0
            8
            GIT_SHORT_SHA)
# Experience shows that explicit package naming can help make it easier to sort
# out potential ABI related issues before they start, while helping you
# track a build to a specific GIT SHA
set(CPACK_PACKAGE_FILE_NAME
    "${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION}-${GIT_SHORT_SHA}-${CMAKE_SYSTEM_NAME}-${CMAKE_BUILD_TYPE}-${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}"
)

include(CPack)