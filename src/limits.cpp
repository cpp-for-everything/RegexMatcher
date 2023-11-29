#include "../lib/limits.hpp"

namespace URLMatcher {
    const Limits Limits::common_edge = Limits();
    const Limits Limits::zero_or_once = Limits(0, 1);
    const Limits Limits::once_or_more = Limits(1, std::nullopt);
    const Limits Limits::zero_or_more = Limits(0, std::nullopt);
}