# Library headers
# .hpp = public headers, .ipp = template implementation (included by .hpp)
file(GLOB_RECURSE LIB_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/matcher/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/matcher/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matcher/impl/*.ipp"
)

# Config header
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/RegexMatcherConfig.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/matcher/RegexMatcherConfig.h
)

# Header-only library (INTERFACE since all code is in headers/ipp files)
add_library(RegexMatcher INTERFACE)
target_sources(RegexMatcher INTERFACE FILE_SET regex_matcher_headers TYPE HEADERS FILES ${LIB_HDRS})
target_include_directories(
    RegexMatcher
    INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Set output properties
set_target_properties(RegexMatcher PROPERTIES EXPORT_NAME core)

# Install headers
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/matcher
    DESTINATION include
    FILES_MATCHING
    PATTERN "*.hpp"
    PATTERN "*.h"
    PATTERN "*.ipp"
)
