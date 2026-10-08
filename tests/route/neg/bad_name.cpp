// expect: parameter name is not
#include <matcher/route.hpp>
struct Ctx {};
void h(std::string_view, Ctx&);
static_assert(matcher::route::check_handler<"/a/{9x}", decltype(&h), Ctx&>());
