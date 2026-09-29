// expect: must start with '/'
#include <matcher/route.hpp>
struct Ctx {};
void h(std::string_view, Ctx&);
static_assert(matcher::route::check_handler<"users", decltype(&h), Ctx&>());
