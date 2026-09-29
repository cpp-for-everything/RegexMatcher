// expect: takes a different number of parameters
#include <matcher/route.hpp>
struct Ctx {};
void h(Ctx&);
static_assert(matcher::route::check_handler<"/u/{id}", decltype(&h), Ctx&>());
