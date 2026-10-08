// expect: compile_time_table_has_duplicate_routes
#include <matcher/route.hpp>
struct Ctx {};
int a(std::string_view, Ctx&);
int b(std::string_view, Ctx&);
using H = matcher::route::handlers<int, Ctx&>;
inline constexpr H::decl kApi[] = {H::get<"/a/{x}", &a>(), H::get<"/a/{y}", &b>()};
inline constexpr auto kTable = matcher::route::make_route_table<kApi>();
