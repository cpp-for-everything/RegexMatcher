// expect: unclosed '{'
#include <matcher/route.hpp>
struct Ctx {};
void h(std::string_view, Ctx&);
static_assert(matcher::route::check_handler<"/users/{id", decltype(&h), Ctx&>());
