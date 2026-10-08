// expect: repeated parameter name
#include <matcher/route.hpp>
struct Ctx {};
void h(std::string_view, Ctx&);
static_assert(matcher::route::check_handler<"/a/{id}/b/{id}", decltype(&h), Ctx&>());
