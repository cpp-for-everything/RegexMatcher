# Negative compilation. Each .cpp here must not compile, for the reason its line
# "// expect: <text>" names: the test builds it and passes only if the build fails and the
# compiler's output contains that text. WILL_FAIL turns the check's failure into the test's
# pass, so a sanitizer run of the suite still reports 100% passed.
file(GLOB _neg_cases CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
foreach(_case IN LISTS _neg_cases)
    get_filename_component(_name "${_case}" NAME_WE)
    file(STRINGS "${_case}" _expect REGEX "^// expect: " LIMIT_COUNT 1)
    string(REGEX REPLACE "^// expect: " "" _expect "${_expect}")
    if(NOT _expect)
        message(FATAL_ERROR "${_case} has no '// expect: ' line")
    endif()
    add_library(route_neg_${_name} OBJECT EXCLUDE_FROM_ALL "${_case}")
    target_link_libraries(route_neg_${_name} PRIVATE RegexMatcher::route)
    add_test(NAME route_neg_compile_${_name}
        COMMAND ${CMAKE_COMMAND} "-DBUILD_DIR=${CMAKE_BINARY_DIR}" "-DTARGET=route_neg_${_name}"
                "-DEXPECT=${_expect}" "-DCONFIG=$<CONFIG>" -P "${CMAKE_CURRENT_SOURCE_DIR}/neg_compile.cmake")
    # One build of this tree at a time: a build directory is not shared by two builds.
    set_tests_properties(route_neg_compile_${_name} PROPERTIES WILL_FAIL TRUE TIMEOUT 600
                         RESOURCE_LOCK route_neg_compile LABELS neg)
endforeach()
