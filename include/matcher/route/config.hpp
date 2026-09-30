#pragma once

// The route matcher's limits and the attributes of its lookup (see matcher/route.hpp).

#include <cstddef>
#include <cstdint>

// The lookup's helpers are inlined into the walk: a call per segment costs more than the
// work it does on short segments.
#if defined(_MSC_VER) && !defined(__clang__)
#define MATCHER_ROUTE_INLINE [[msvc::forceinline]]
#define MATCHER_ROUTE_NOINLINE [[msvc::noinline]]
#else
#define MATCHER_ROUTE_INLINE [[gnu::always_inline]]
#define MATCHER_ROUTE_NOINLINE [[gnu::noinline]]
#endif

// Instrumentation, none by default: the walk calls MATCHER_ROUTE_ON_BACKTRACK() at every pop of
// its backtracking stack, each time it leaves a branch it went down to try another. A program
// that counts them defines it before it includes matcher/route.hpp. It is never called in
// constant evaluation.
#ifndef MATCHER_ROUTE_ON_BACKTRACK
#define MATCHER_ROUTE_ON_BACKTRACK()
#endif

namespace matcher::route
{

	inline constexpr std::size_t kMaxSegments = 32;
	inline constexpr std::size_t kMaxParams = 16;
	inline constexpr std::size_t kMethodSlots = 9;  // GET, POST, PUT, DELETE, PATCH, HEAD, OPTIONS, CONNECT, TRACE
	inline constexpr std::uint32_t kNone = 0xFFFF'FFFFu;

}  // namespace matcher::route
