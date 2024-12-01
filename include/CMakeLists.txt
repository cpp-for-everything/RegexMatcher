# Library sources
file(GLOB_RECURSE LIB_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
file(GLOB_RECURSE LIB_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/impl/*.cpp")

# Core library
add_library(regexmatcher_core STATIC ${LIB_HDRS} ${LIB_SRCS})
target_include_directories(
    regexmatcher_core
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
    # ${CMAKE_BINARY_DIR}/config
)
set_target_properties(regexmatcher_core PROPERTIES LINKER_LANGUAGE CXX)

# Set output properties
set_target_properties(regexmatcher_core PROPERTIES EXPORT_NAME core)

# Install targets and headers
install(
    TARGETS regexmatcher_core
    EXPORT RegexMatcherTargets
    DESTINATION lib
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)
