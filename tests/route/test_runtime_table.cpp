// Tables built at run time in one block (RuntimeTable): the table build_table builds, with its
// four arrays at the offsets a compile-time table of the same routes has, in a block aligned to
// a page, so that a compile-time table declared alignas(kPageAlign) and a run-time table put
// every element at the same offset into a page.
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <matcher/route.hpp>

#include "route_test_util.hpp"

using namespace matcher::route;
using route_test::expect_same_contents;
using route_test::expect_same_page_offsets;
using route_test::GET;
using route_test::page_offset;
using route_test::POST;
using route_test::PUT;
using route_test::values;

namespace
{
	// Every route literal: an exact-match table and no trie (no nodes, no edges).
	constexpr RouteSpec kLiteralRoutes[] = {
		{GET, "/", 0},
		{GET, "/a/b", 1},
		{POST, "/a/b", 2},
		{GET, "/c", 3},
	};
	// Every route with a parameter: a trie and no exact-match table.
	constexpr RouteSpec kTrieRoutes[] = {
		{GET, "/a/{x}", 0},
		{GET, "/a/{x}/c/{*rest}", 1},
		{GET, "/users/{id:u64}", 2},
		{PUT, "/users/{name}", 3},
	};
	// Both: POST's routes are all literal, GET's are not.
	constexpr RouteSpec kMixedRoutes[] = {
		{GET, "/", 0},
		{GET, "/a/b", 1},
		{GET, "/a/{x}", 2},
		{GET, "/a/{x}/c/{*rest}", 3},
		{POST, "/a/b", 4},
		{GET, "/users/{id:u64}", 5},
		{GET, "/users/{name}", 6},
	};

	alignas(kPageAlign) constexpr auto kLiteral = make_static_table<kLiteralRoutes>();
	alignas(kPageAlign) constexpr auto kTrie = make_static_table<kTrieRoutes>();
	alignas(kPageAlign) constexpr auto kMixed = make_static_table<kMixedRoutes>();

	static_assert(kLiteral.view().nodes.empty() && kLiteral.view().edges.empty() && !kLiteral.view().literals.empty());
	static_assert(!kTrie.view().nodes.empty() && !kTrie.view().edges.empty() && kTrie.view().literals.empty());
	static_assert(!kMixed.view().nodes.empty() && !kMixed.view().literals.empty());

	// The layout a compile-time table's members have is the one table_layout computes.
	template <StaticSizes S>
	constexpr bool layout_matches()
	{
		constexpr TableLayout l = table_layout(S);
		return offsetof(StaticTable<S>, nodes) == l.nodes && offsetof(StaticTable<S>, edges) == l.edges &&
		       offsetof(StaticTable<S>, literals) == l.literals && offsetof(StaticTable<S>, arena) == l.arena &&
		       l.bytes == l.arena + S.arena + 1;
	}
	static_assert(layout_matches<StaticSizes{}>());
	static_assert(layout_matches<StaticSizes{0, 0, 8, 17}>());
	static_assert(layout_matches<StaticSizes{3, 0, 0, 1}>());
	static_assert(layout_matches<StaticSizes{5, 7, 0, 100}>());
	static_assert(layout_matches<StaticSizes{1, 1, 1, 0}>());
	static_assert(layout_matches<StaticSizes{1000, 3000, 2048, 65536}>());

	template <auto& Routes, class Static>
	void check_table(const Static& compiled)
	{
		const std::span<const RouteSpec> specs(Routes);
		const Built built = build_table(specs);
		ASSERT_EQ(built.error, BuildError::None);
		const RuntimeTable t = make_runtime_table(specs);
		ASSERT_EQ(t.error, BuildError::None);
		const TableView v = t.view();
		expect_same_contents(v, built.view());
		expect_same_contents(v, compiled.view());
		expect_same_page_offsets(v, compiled.view());

		// One block, aligned to a page, holding the arrays where table_layout puts them.
		ASSERT_NE(t.data(), nullptr);
		EXPECT_EQ(page_offset(t.data()), 0u);
		const TableLayout l = table_layout({v.nodes.size(), v.edges.size(), v.literals.size(), v.arena.size()});
		EXPECT_EQ(t.bytes(), l.bytes);
		const auto at = [&t](const void* p) { return static_cast<std::size_t>(static_cast<const std::byte*>(p) - t.data()); };
		if (!v.nodes.empty())
		{
			EXPECT_EQ(at(v.nodes.data()), l.nodes);
		}
		if (!v.edges.empty())
		{
			EXPECT_EQ(at(v.edges.data()), l.edges);
		}
		if (!v.literals.empty())
		{
			EXPECT_EQ(at(v.literals.data()), l.literals);
		}
		EXPECT_EQ(at(v.arena.data()), l.arena);

		for (unsigned method : {GET, POST, PUT})
		{
			for (const char* path : {"/", "/a", "/a/b", "/a/q", "/a/q/c/r", "/a/q/c", "/c", "/users/12", "/users/bob",
			                         "/users/", "/x"})
			{
				SCOPED_TRACE(path);
				const Match r = find(v, method, path);
				const Match s = find(built.view(), method, path);
				EXPECT_EQ(r.status, s.status);
				EXPECT_EQ(r.route, s.route);
				EXPECT_EQ(values(r), values(s));
				EXPECT_EQ(r.allowed, s.allowed);
			}
		}
	}
}  // namespace

TEST(RouteRuntimeTable, LiteralRoutesOnly)
{
	check_table<kLiteralRoutes>(kLiteral);
}

TEST(RouteRuntimeTable, ParameterRoutesOnly)
{
	check_table<kTrieRoutes>(kTrie);
}

TEST(RouteRuntimeTable, LiteralAndParameterRoutes)
{
	check_table<kMixedRoutes>(kMixed);
}

TEST(RouteRuntimeTable, NoRoutes)
{
	const RuntimeTable t = make_runtime_table({});
	ASSERT_EQ(t.error, BuildError::None);
	EXPECT_EQ(page_offset(t.data()), 0u);
	EXPECT_EQ(t.bytes(), table_layout({}).bytes);
	EXPECT_EQ(find(t.view(), GET, "/").status, Status::NotFound);
}

TEST(RouteRuntimeTable, ABadRouteGivesTheBuildErrorAndNoBlock)
{
	const std::vector<RouteSpec> specs = {{GET, "/a", 0}, {GET, "/b/{x", 1}};
	const RuntimeTable t = make_runtime_table(specs);
	EXPECT_EQ(t.error, BuildError::BadPattern);
	EXPECT_EQ(t.error_route, 1u);
	EXPECT_EQ(t.data(), nullptr);
	EXPECT_EQ(t.bytes(), 0u);

	const std::vector<RouteSpec> dup = {{GET, "/a/{x}", 0}, {GET, "/a/{y}", 1}};
	const RuntimeTable d = make_runtime_table(dup);
	EXPECT_EQ(d.error, BuildError::Duplicate);
	EXPECT_EQ(d.error_route, 1u);
	EXPECT_EQ(d.error_other, 0u);
	EXPECT_EQ(d.data(), nullptr);
}

TEST(RouteRuntimeTable, MovingATableMovesItsBlock)
{
	RuntimeTable a = make_runtime_table(kMixedRoutes);
	const std::byte* block = a.data();
	const TableView before = a.view();
	RuntimeTable b = std::move(a);
	EXPECT_EQ(b.data(), block);
	EXPECT_EQ(b.view().nodes.data(), before.nodes.data());
	EXPECT_EQ(a.data(), nullptr);  // NOLINT(bugprone-use-after-move): a moved-from table is empty
	EXPECT_TRUE(a.view().nodes.empty() && a.view().literals.empty() && a.view().arena.empty());
	EXPECT_EQ(find(b.view(), GET, "/a/b").route, 1u);

	RuntimeTable c = make_runtime_table(kTrieRoutes);
	c = std::move(b);
	EXPECT_EQ(c.data(), block);
	EXPECT_EQ(find(c.view(), POST, "/a/b").route, 4u);
}
