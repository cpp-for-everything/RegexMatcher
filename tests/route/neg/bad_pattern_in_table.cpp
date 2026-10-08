// expect: compile_time_table_has_a_bad_pattern
#include <matcher/route.hpp>
using matcher::route::RouteSpec;
constexpr RouteSpec kRoutes[] = {{0, "/a/{x", 0}};
constexpr auto kTable = matcher::route::make_static_table<kRoutes>();
