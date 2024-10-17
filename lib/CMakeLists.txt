add_library(regexmatcher_core STATIC 
    src/node_info.cpp
)
target_sources(regexmatcher_core
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_BINARY_DIR}/config/RegexMatcherConfig.h
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(regexmatcher_core PUBLIC include ${CMAKE_BINARY_DIR}/config)

install(TARGETS regexmatcher_core DESTINATION lib)
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include
    DESTINATION include/
)
install(FILES ${CMAKE_BINARY_DIR}/config/RegexMatcherConfig.h DESTINATION include/)

add_library(RegexMatcher::core ALIAS regexmatcher_core)
