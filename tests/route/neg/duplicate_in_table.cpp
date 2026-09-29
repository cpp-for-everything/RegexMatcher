// expect: compile_time_table_has_duplicate_routes
#include <matcher/route.hpp>
using matcher::route::RouteSpec;
constexpr RouteSpec kRoutes[] = {{0, "/a/{x}", 0}, {0, "/a/{y}", 1}};
constexpr auto kTable = matcher::route::make_static_table<kRoutes>();
