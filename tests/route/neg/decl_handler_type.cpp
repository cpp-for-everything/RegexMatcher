// expect: its parameters do not match the route pattern
#include <matcher/route.hpp>
struct Ctx {};
int h(std::string_view, Ctx&);
using H = matcher::route::handlers<int, Ctx&>;
inline constexpr H::decl kApi[] = {H::get<"/u/{id:u64}", &h>()};
