file(GLOB_RECURSE _HDRS "include/matcher/*.(h|hpp)")
file(GLOB_RECURSE _SRCS "include/matcher/impl/*.cpp")
add_library(core STATIC)
target_sources(core
    PUBLIC ${_HDRS} ${CMAKE_BINARY_DIR}/config/RegexMatcherConfig.h
    PRIVATE ${_SRCS}
)
target_include_directories(core PUBLIC include ${CMAKE_BINARY_DIR}/config)

install(TARGETS core DESTINATION lib)
install(FILES ${_HDRS} ${CMAKE_BINARY_DIR}/config/RegexMatcherConfig.h DESTINATION include/matcher)