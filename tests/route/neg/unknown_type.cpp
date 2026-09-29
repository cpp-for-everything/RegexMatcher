// expect: unknown parameter type
#include <matcher/route.hpp>
struct Ctx {};
void h(std::string_view, Ctx&);
static_assert(matcher::route::check_handler<"/a/{id:float}", decltype(&h), Ctx&>());
