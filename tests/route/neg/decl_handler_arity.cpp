// expect: takes a different number of parameters
#include <matcher/route.hpp>
struct Ctx {};
int h(Ctx&);
using H = matcher::route::handlers<int, Ctx&>;
inline constexpr H::decl kApi[] = {H::get<"/u/{id}", &h>()};
