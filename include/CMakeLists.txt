# Library headers
# .hpp = public headers, .ipp = template implementation (included by .hpp)

# Config header, written before the headers are listed so that the engine's file set, and so
# the installed package, holds it (core.hpp includes it).
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/RegexMatcherConfig.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/matcher/RegexMatcherConfig.h
)

# The regex engine: matcher/*.hpp and matcher/impl/*.ipp, without the route matcher.
file(GLOB LIB_HDRS
    "${CMAKE_CURRENT_SOURCE_DIR}/matcher/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/matcher/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/matcher/impl/*.ipp"
)
list(REMOVE_ITEM LIB_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/matcher/route.hpp")

# The route matcher: matcher/route.hpp and everything under matcher/route/.
file(GLOB_RECURSE ROUTE_HDRS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/matcher/route/*.hpp")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/matcher/route.hpp")
    list(APPEND ROUTE_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/matcher/route.hpp")
endif()

# The regex engine, header-only (INTERFACE since all code is in headers/ipp files), C++17
add_library(RegexMatcher INTERFACE)
add_library(RegexMatcher::core ALIAS RegexMatcher)
target_sources(RegexMatcher INTERFACE FILE_SET regex_matcher_headers TYPE HEADERS FILES ${LIB_HDRS})
target_include_directories(
    RegexMatcher
    INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(RegexMatcher INTERFACE cxx_std_17)
set_target_properties(RegexMatcher PROPERTIES EXPORT_NAME core)

# The route matcher, header-only, C++23, standard library only
add_library(RegexMatcherRoute INTERFACE)
add_library(RegexMatcher::route ALIAS RegexMatcherRoute)
target_sources(RegexMatcherRoute INTERFACE FILE_SET route_headers TYPE HEADERS FILES ${ROUTE_HDRS})
target_include_directories(
    RegexMatcherRoute
    INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(RegexMatcherRoute INTERFACE cxx_std_23)
set_target_properties(RegexMatcherRoute PROPERTIES EXPORT_NAME route)
