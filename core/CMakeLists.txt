file(GLOB_RECURSE _HDRS "include/matcher/*.(h|hpp)")
file(GLOB_RECURSE _SRCS "include/matcher/impl/*.cpp")
add_library(regexmatcher_core STATIC)
target_sources(regexmatcher_core
    PUBLIC ${_HDRS} ${CMAKE_BINARY_DIR}/config/RegexMatcherConfig.h
    PRIVATE ${_SRCS}
)
target_include_directories(regexmatcher_core PUBLIC include ${CMAKE_BINARY_DIR}/config)

install(TARGETS regexmatcher_core DESTINATION lib)
install(FILES ${_HDRS} ${CMAKE_BINARY_DIR}/config/RegexMatcherConfig.h DESTINATION include/matcher)

add_library(RegexMatcher::core ALIAS regexmatcher_core)
