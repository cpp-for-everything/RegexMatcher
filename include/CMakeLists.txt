# Library sources
file(GLOB_RECURSE LIB_HDRS "${CMAKE_CURRENT_SOURCE_DIR}/matcher/*.hpp" "${CMAKE_CURRENT_SOURCE_DIR}/matcher/*.h")
file(GLOB_RECURSE LIB_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/matcher/impl/*.cpp")

# Config header
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/RegexMatcherConfig.h.in ${CMAKE_CURRENT_SOURCE_DIR}/matcher/RegexMatcherConfig.h)

# Core library
add_library(RegexMatcher STATIC ${LIB_SRCS})
target_sources(RegexMatcher PUBLIC FILE_SET regex_matcher_headers TYPE HEADERS FILES ${LIB_HDRS})
target_include_directories(
    RegexMatcher
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(RegexMatcher PROPERTIES LINKER_LANGUAGE CXX)

# Set output properties
set_target_properties(RegexMatcher PROPERTIES EXPORT_NAME core)

install(FILES ${LIB_SRCS}
    DESTINATION lib/RegexMatcher-${PROJECT_VERSION}/matcher/impl
)
