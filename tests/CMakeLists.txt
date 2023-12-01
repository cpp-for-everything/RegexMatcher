file(GLOB_RECURSE _SRCS "src/*.[hc]pp")
add_executable(tests EXCLUDE_FROM_ALL ${_SRCS})
target_link_libraries(tests core)
target_compile_features(tests PRIVATE cxx_std_17)

install(CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\" --build \"${CMAKE_CURRENT_BINARY_DIR}\" --target tests)")
install(TARGETS tests DESTINATION bin)