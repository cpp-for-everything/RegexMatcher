// The checked front end: methods, patterns as template arguments, the C++ type of each
// parameter, handler checks, conversion of the captured values, and compile-time tables of
// route declarations with handlers.
#include <gtest/gtest.h>

#include <concepts>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>

#include <matcher/route.hpp>

#include "route_test_util.hpp"

using namespace matcher::route;

namespace
{
	struct Ctx
	{
		int calls = 0;
		std::string seen;
	};

	std::string get_user(std::uint64_t id, Ctx& c)
	{
		++c.calls;
		return "user " + std::to_string(id);
	}

	std::string get_file(std::string_view name, std::string_view rest, Ctx& c)
	{
		++c.calls;
		return std::string(name) + ":" + std::string(rest);
	}

	using H = handlers<std::string, Ctx&>;
	inline constexpr H::decl kApi[] = {
		H::get<"/users/{id:u64}", &get_user>(),
		H::get<"/files/{name}/{*rest}", &get_file>(),
		H::post<"/users/{id:u64}", &get_user>(),
	};
	inline constexpr auto kApiTable = make_route_table<kApi>();
}  // namespace

TEST(RouteChecked, MethodValuesAreTheTableIndices)
{
	static_assert(static_cast<unsigned>(Method::Get) == 0);
	static_assert(static_cast<unsigned>(Method::Post) == 1);
	static_assert(static_cast<unsigned>(Method::Put) == 2);
	static_assert(static_cast<unsigned>(Method::Delete) == 3);
	static_assert(static_cast<unsigned>(Method::Patch) == 4);
	static_assert(static_cast<unsigned>(Method::Head) == 5);
	static_assert(static_cast<unsigned>(Method::Options) == 6);
	static_assert(static_cast<unsigned>(Method::Connect) == 7);
	static_assert(static_cast<unsigned>(Method::Trace) == 8);
	static_assert(kMethodSlots == 9);
	const Built b = route_test::table({{1u, "/a/{x}", 0}, {0u, "/b", 0}});
	for (const char* path : {"/a/q", "/b", "/c"})
	{
		const Match by_enum = find(b.view(), Method::Post, path);
		const Match by_index = find(b.view(), 1u, path);
		EXPECT_EQ(by_enum.status, by_index.status);
		EXPECT_EQ(by_enum.route, by_index.route);
		Match into;
		find_into(b.view(), Method::Post, path, into);
		EXPECT_EQ(into.status, by_index.status);
		EXPECT_EQ(into.route, by_index.route);
	}
}

TEST(RouteChecked, AStringLiteralIsATemplateArgument)
{
	constexpr fixed_string s = "/a/{b}";
	static_assert(s.view() == "/a/{b}");
	static_assert(checked_pattern<"/users/{id:u64}/files">::value.n == 3);
	static_assert(checked_pattern<"/users/{id:u64}/files">::value.params == 1);
}

TEST(RouteChecked, EachParameterHasItsCppType)
{
	static_assert(std::same_as<param_t<"/a/{x}/{n:u64}/{i:i64}/{*r}", 0>, std::string_view>);
	static_assert(std::same_as<param_t<"/a/{x}/{n:u64}/{i:i64}/{*r}", 1>, std::uint64_t>);
	static_assert(std::same_as<param_t<"/a/{x}/{n:u64}/{i:i64}/{*r}", 2>, std::int64_t>);
	static_assert(std::same_as<param_t<"/a/{x}/{n:u64}/{i:i64}/{*r}", 3>, std::string_view>);
	static_assert(std::same_as<param_t<"/a/{x:string}", 0>, std::string_view>);
}

TEST(RouteChecked, HandlersThatFitTheirPatternAreAccepted)
{
	static_assert(check_handler<"/u/{id:u64}", std::string (*)(std::uint64_t, Ctx&), Ctx&>());
	static_assert(check_handler<"/a/{x}", void (*)(std::string_view)>());
	static_assert(check_handler<"/", int (*)()>());
	const auto lambda = [](std::int64_t, std::string_view, int) noexcept { return 1; };
	static_assert(check_handler<"/t/{n:i64}/{s}", decltype(lambda), int>());
	const auto generic = [](auto, auto&) {};
	static_assert(check_handler<"/g/{x}", decltype(generic), Ctx&>());
}

TEST(RouteChecked, InvokeConvertsTheCapturedValues)
{
	const Built b = route_test::table({{0u, "/n/{a:u64}/{b:i64}/{s}", 0}});
	const std::string path = "/n/18446744073709551615/-9223372036854775808/zz";
	const Match m = find(b.view(), Method::Get, path);
	ASSERT_EQ(m.status, Status::Found);
	std::uint64_t a = 0;
	std::int64_t bb = 0;
	std::string s;
	constexpr auto h = [](std::uint64_t x, std::int64_t y, std::string_view z, std::uint64_t& ra, std::int64_t& rb,
	                      std::string& rs) {
		ra = x;
		rb = y;
		rs = z;
		return 7;
	};
	const int r = invoke<"/n/{a:u64}/{b:i64}/{s}", h, std::uint64_t&, std::int64_t&, std::string&>(m.values(), a, bb, s);
	EXPECT_EQ(r, 7);
	EXPECT_EQ(a, std::numeric_limits<std::uint64_t>::max());
	EXPECT_EQ(bb, std::numeric_limits<std::int64_t>::min());
	EXPECT_EQ(s, "zz");
}

TEST(RouteChecked, ACompileTimeTableOfDeclarationsCallsItsHandlers)
{
	static_assert(kApiTable.table.find(0u, "/users/5").route == 0);
	static_assert(kApiTable.table.find(0u, "/files/a/b/c").route == 1);
	static_assert(kApiTable.table.find(1u, "/users/5").route == 2);
	static_assert(kApiTable.table.find(2u, "/users/5").status == Status::MethodNotAllowed);
	Ctx c;
	const std::string p1 = "/users/42";
	const Match m1 = kApiTable.table.find(0u, p1);
	ASSERT_EQ(m1.status, Status::Found);
	EXPECT_EQ(kApiTable.routes[m1.route].handler(m1.values(), c), "user 42");
	const std::string p2 = "/files/readme/a/b";
	const Match m2 = kApiTable.table.find(0u, p2);
	ASSERT_EQ(m2.status, Status::Found);
	EXPECT_EQ(kApiTable.routes[m2.route].handler(m2.values(), c), "readme:a/b");
	EXPECT_EQ(c.calls, 2);
	EXPECT_EQ(kApiTable.routes[2].method, 1u);
	EXPECT_EQ(kApiTable.routes[1].pattern, "/files/{name}/{*rest}");
}
