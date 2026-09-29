# Compile-time routing tables at the sizes the P2 paper measures (ct_table_case.hpp): every
# table shape at m = 10, 100 and 1,000, and the GitHub API table, each built while compiling
# and at run time and checked against the same queries.
#
# Constant evaluation of a 1,000-route table takes more steps than compilers allow by default,
# and a lot of compiler memory, so these translation units get REGEXMATCHER_CT_STEPS and
# compile REGEXMATCHER_CT_JOBS at a time (a Ninja job pool; other generators ignore it).
set(REGEXMATCHER_CT_STEPS 33554432 CACHE STRING "constant-evaluation budget of the compile-time-table tests")
set(REGEXMATCHER_CT_JOBS 1 CACHE STRING "compile-time-table test translation units compiled at once")
set_property(GLOBAL APPEND PROPERTY JOB_POOLS regexmatcher_ct_tables=${REGEXMATCHER_CT_JOBS})

file(GLOB _ct_table_tests CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")
add_executable(route_ct_table_tests ${_ct_table_tests})
set_property(TARGET route_ct_table_tests PROPERTY JOB_POOL_COMPILE regexmatcher_ct_tables)
target_link_libraries(route_ct_table_tests PRIVATE RegexMatcher::route GTest::gtest_main)
target_include_directories(route_ct_table_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(route_ct_table_tests PRIVATE "/constexpr:steps${REGEXMATCHER_CT_STEPS}" /bigobj)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    target_compile_options(route_ct_table_tests PRIVATE "/clang:-fconstexpr-steps=${REGEXMATCHER_CT_STEPS}" /bigobj)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(route_ct_table_tests PRIVATE "-fconstexpr-steps=${REGEXMATCHER_CT_STEPS}")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    # GCC counts operations, not steps; its limits are raised by the same factor over its
    # defaults (2^33 operations, 262,144 loop iterations) as clang's is.
    target_compile_options(route_ct_table_tests PRIVATE -fconstexpr-ops-limit=274877906944 -fconstexpr-loop-limit=8388608)
endif()
gtest_discover_tests(route_ct_table_tests DISCOVERY_MODE PRE_TEST)
