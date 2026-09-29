// expect: is not the last segment
#include <matcher/route.hpp>
struct Ctx {};
void h(std::string_view, Ctx&);
static_assert(matcher::route::check_handler<"/f/{*p}/x", decltype(&h), Ctx&>());
