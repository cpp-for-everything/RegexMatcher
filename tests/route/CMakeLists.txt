# The route matcher's tests (include/matcher/route.hpp):
#   route_tests            behaviour of the pattern parser, the table and the lookup
#   route_checked_tests    the checked front end (methods, patterns as template arguments,
#                          handler checks, tables of declarations with handlers)
#   route_ct_table_tests   compile-time tables at the sizes of the route-matching benchmark
#                          (paper-typed-routing; ct_tables/)
#   route_neg_compile_*    negative compilation: each malformed route fails for its reason (neg/)
add_executable(route_tests
    test_pattern.cpp
    test_lookup.cpp
    test_table.cpp
    test_static_table.cpp
    test_runtime_table.cpp
    test_words.cpp
)
add_executable(route_checked_tests test_checked.cpp)
foreach(_t route_tests route_checked_tests)
    target_link_libraries(${_t} PRIVATE RegexMatcher::route GTest::gtest_main)
    if(MSVC)
        target_compile_options(${_t} PRIVATE /W4 /bigobj)
    else()
        target_compile_options(${_t} PRIVATE -Wall -Wextra)
    endif()
    gtest_discover_tests(${_t} DISCOVERY_MODE PRE_TEST)
endforeach()

add_subdirectory(ct_tables)
add_subdirectory(neg)
