// expect: its parameters do not match the route pattern
#include <matcher/route.hpp>
struct Ctx {};
void h(std::string_view, Ctx&);
static_assert(matcher::route::check_handler<"/u/{id:u64}", decltype(&h), Ctx&>());
