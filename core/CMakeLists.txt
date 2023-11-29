file(GLOB_RECURSE _HDRS "include/*.(h|hpp)")
file(GLOB_RECURSE _SRCS "include/*.cpp")
add_library(core STATIC)
target_sources(core
    PUBLIC ${_HDRS} ${CMAKE_BINARY_DIR}/RegexMatcherConfig.h
    PRIVATE ${_SRCS}
)
target_include_directories(core PUBLIC include ${CMAKE_BINARY_DIR}/config)
target_compile_features(core PRIVATE cxx_std_17)

install(TARGETS core DESTINATION lib)
# install(FILES ${_HDRS} DESTINATION include/matcher)