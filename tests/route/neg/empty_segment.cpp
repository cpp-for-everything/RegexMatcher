// expect: empty segment
#include <matcher/route.hpp>
struct Ctx {};
void h(std::string_view, Ctx&);
static_assert(matcher::route::check_handler<"/a//b", decltype(&h), Ctx&>());
