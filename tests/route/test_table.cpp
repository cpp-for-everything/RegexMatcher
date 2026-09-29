// Building a table: duplicates and malformed routes are errors that name the route.
#include <gtest/gtest.h>

#include <matcher/route.hpp>

#include "route_test_util.hpp"

using namespace matcher::route;
using route_test::GET;
using route_test::POST;

TEST(RouteTable, TwoRoutesOfOneMethodThatMatchTheSamePathsAreRejected)
{
	const RouteSpec same_names[] = {{GET, "/a/{x}", 0}, {GET, "/a/{y}", 1}};
	const Built a = build_table(same_names);
	EXPECT_EQ(a.error, BuildError::Duplicate);
	EXPECT_EQ(a.error_route, 1u);
	EXPECT_EQ(a.error_other, 0u);
	const RouteSpec literal[] = {{GET, "/a/b", 0}, {GET, "/a/b", 1}};
	EXPECT_EQ(build_table(literal).error, BuildError::Duplicate);
	const RouteSpec methods[] = {{GET, "/a/{x}", 0}, {POST, "/a/{y}", 1}};
	EXPECT_EQ(build_table(methods).error, BuildError::None);
	const RouteSpec types[] = {{GET, "/a/{x:u64}", 0}, {GET, "/a/{y:i64}", 1}, {GET, "/a/{z}", 2}};
	EXPECT_EQ(build_table(types).error, BuildError::None);
	const RouteSpec bad[] = {{GET, "/a/{x", 0}};
	EXPECT_EQ(build_table(bad).error, BuildError::BadPattern);
	const RouteSpec bad_method[] = {{kMethodSlots, "/a", 0}};
	EXPECT_EQ(build_table(bad_method).error, BuildError::BadMethod);
}
