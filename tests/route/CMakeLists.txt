# The route matcher's tests (include/matcher/route.hpp):
#   route_tests            behaviour of the pattern parser, the table and the lookup, and the
#                          checked front end
#   route_ct_table_tests   compile-time tables at the sizes the P2 paper measures (ct_tables/)
#   route_neg_compile_*    negative compilation: each malformed route fails for its reason (neg/)
add_executable(route_tests
    test_pattern.cpp
    test_lookup.cpp
    test_table.cpp
    test_static_table.cpp
    test_words.cpp
    test_checked.cpp
)
target_link_libraries(route_tests PRIVATE RegexMatcher::route GTest::gtest_main)
if(MSVC)
    target_compile_options(route_tests PRIVATE /W4 /bigobj)
else()
    target_compile_options(route_tests PRIVATE -Wall -Wextra)
endif()
gtest_discover_tests(route_tests DISCOVERY_MODE PRE_TEST)

add_subdirectory(ct_tables)
add_subdirectory(neg)
