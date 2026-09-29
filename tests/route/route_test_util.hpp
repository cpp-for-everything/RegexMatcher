// Helpers shared by the route matcher's tests.
#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include <matcher/route.hpp>

namespace route_test
{
	inline constexpr unsigned GET = 0;
	inline constexpr unsigned POST = 1;
	inline constexpr unsigned PUT = 2;

	// A table of the routes, each route's value its index in registration order.
	inline matcher::route::Built table(std::vector<matcher::route::RouteSpec> specs)
	{
		for (std::uint32_t i = 0; i < specs.size(); ++i)
		{
			specs[i].route = i;
		}
		matcher::route::Built b = matcher::route::build_table(specs);
		EXPECT_EQ(b.error, matcher::route::BuildError::None);
		return b;
	}

	inline std::vector<std::string> values(const matcher::route::Match& m)
	{
		return {m.params.begin(), m.params.begin() + m.count};
	}
}  // namespace route_test
