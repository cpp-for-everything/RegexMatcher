// Tables built while compiling: the same answers as a table built at run time.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <matcher/route.hpp>

#include "route_test_util.hpp"

using namespace matcher::route;
using route_test::GET;
using route_test::POST;
using route_test::PUT;
using route_test::values;

namespace
{
	constexpr RouteSpec kStaticRoutes[] = {
		{GET, "/", 0},
		{GET, "/a/b", 1},
		{GET, "/a/{x}", 2},
		{GET, "/a/{x}/c/{*rest}", 3},
		{POST, "/a/b", 4},
		{GET, "/users/{id:u64}", 5},
		{GET, "/users/{name}", 6},
	};
	constexpr auto kStatic = make_static_table<kStaticRoutes>();

	static_assert(kStatic.find(GET, "/").route == 0);
	static_assert(kStatic.find(GET, "/a/b").route == 1);
	static_assert(kStatic.find(GET, "/a/zz").route == 2);
	static_assert(kStatic.find(GET, "/a/zz/c/d/e").route == 3);
	static_assert(kStatic.find(POST, "/a/b").route == 4);
	static_assert(kStatic.find(PUT, "/a/b").status == Status::MethodNotAllowed);
	static_assert(kStatic.find(GET, "/users/7").route == 5);
	static_assert(kStatic.find(GET, "/users/x").route == 6);
	static_assert(kStatic.find(GET, "/nope").status == Status::NotFound);
}  // namespace

TEST(RouteStaticTable, ACompileTimeTableAnswersAsTheRunTimeTableDoes)
{
	const Built b = build_table(kStaticRoutes);
	ASSERT_EQ(b.error, BuildError::None);
	for (unsigned method : {GET, POST, PUT})
	{
		for (const char* path : {"/", "/a", "/a/b", "/a/q", "/a/q/c/r", "/a/q/c", "/users/12", "/users/bob",
		                         "/users/", "/x"})
		{
			SCOPED_TRACE(path);
			const Match r = find(b.view(), method, path);
			const Match s = kStatic.find(method, path);
			EXPECT_EQ(r.status, s.status);
			EXPECT_EQ(r.route, s.route);
			EXPECT_EQ(values(r), values(s));
			EXPECT_EQ(r.allowed, s.allowed);
		}
	}
}
