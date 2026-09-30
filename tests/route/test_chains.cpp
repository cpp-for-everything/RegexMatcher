// Chained edges: in a linear node, a run of literal segments in which each segment's node has no
// route and one literal child only is one edge, its label the whole run, and the node is of the
// kind "linear with chained edges" (kNodeChained). The lookup answers exactly as over the trie
// without chains: every answer below is the one the routes' semantics give.
#include <gtest/gtest.h>

#include <string>
#include <string_view>
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
	// The edge of node n whose first segment is `first`, or null.
	const Edge* edge_to(const TableView& v, const Node& n, std::string_view first)
	{
		for (std::uint32_t i = n.edges; i < n.edges + n.count; ++i)
		{
			const Edge& e = v.edges[i];
			if (e.child != kNone && v.arena.substr(e.off, e.len) == first)
			{
				return &e;
			}
		}
		return nullptr;
	}

	std::string_view tail_of(const TableView& v, const Edge& e)
	{
		return v.arena.substr(e.off + e.len, e.tail);
	}

	// The route a lookup finds (kNone: none) and its captured values.
	std::pair<std::uint32_t, std::vector<std::string>> at(const Built& b, unsigned method, std::string_view path)
	{
		const Match m = find(b.view(), method, path);
		return {m.status == Status::Found ? m.route : kNone, values(m)};
	}

	using Values = std::vector<std::string>;

	// The chain of the root's edge `first` for method GET (a method whose routes are all literal
	// has no trie, so every table below has a parameter route too).
	std::string root_chain(const Built& b, std::string_view first)
	{
		const TableView v = b.view();
		if (v.roots[GET] == kNone)
		{
			return "no trie";
		}
		const Edge* e = edge_to(v, v.nodes[v.roots[GET]], first);
		return e == nullptr ? "no edge" : std::string(tail_of(v, *e));
	}
}  // namespace

TEST(RouteChains, ARunOfSingleChildLiteralSegmentsIsOneEdge)
{
	const Built b = route_test::table({{GET, "/api/v1/users/{id}"}, {GET, "/api/v1/users/me"}, {GET, "/health"}});
	const TableView v = b.view();
	const Node& root = v.nodes[v.roots[GET]];
	EXPECT_EQ(root.hashed, kNodeChained);
	const Edge* api = edge_to(v, root, "api");
	ASSERT_NE(api, nullptr);
	EXPECT_EQ(tail_of(v, *api), "/v1/users");
	const Node& users = v.nodes[api->child];
	EXPECT_NE(users.param, kNone);
	EXPECT_EQ(users.count, 1u);
	EXPECT_EQ(users.hashed, kNodeLinear);
	const Edge* health = edge_to(v, root, "health");
	ASSERT_NE(health, nullptr);
	EXPECT_EQ(health->tail, 0u);
	EXPECT_EQ(v.nodes.size(), 5u);  // the root, users, me, {id}, health
	EXPECT_EQ(at(b, GET, "/api/v1/users/7"), std::make_pair(0u, Values{"7"}));
	EXPECT_EQ(at(b, GET, "/api/v1/users/me"), std::make_pair(1u, Values{}));
	EXPECT_EQ(at(b, GET, "/health"), std::make_pair(2u, Values{}));
}

TEST(RouteChains, OnlyLinearNodesHaveChainedEdges)
{
	// Nine first segments: the root is hashed and its edges are not chained; each of its
	// children is a linear node whose one edge is.
	std::vector<RouteSpec> specs;
	std::vector<std::string> patterns;
	for (char c = 'a'; c < 'a' + 9; ++c)
	{
		patterns.push_back(std::string("/") + c + "/x/y");
	}
	for (const std::string& p : patterns)
	{
		specs.push_back({GET, p, 0});
	}
	specs.push_back({GET, "/zz/{q}", 0});
	const Built b = route_test::table(specs);
	const TableView v = b.view();
	const Node& root = v.nodes[v.roots[GET]];
	EXPECT_EQ(root.hashed, kNodeHashed);
	for (std::uint32_t i = root.edges; i < root.edges + root.count; ++i)
	{
		const Edge& e = v.edges[i];
		if (e.child == kNone)
		{
			continue;
		}
		EXPECT_EQ(e.tail, 0u);
		const Node& child = v.nodes[e.child];
		if (v.arena.substr(e.off, e.len) == "zz")
		{
			continue;
		}
		EXPECT_EQ(child.hashed, kNodeChained);
		const Edge* x = edge_to(v, child, "x");
		ASSERT_NE(x, nullptr);
		EXPECT_EQ(tail_of(v, *x), "/y");
	}
	EXPECT_EQ(at(b, GET, "/e/x/y").first, 4u);
	EXPECT_EQ(at(b, GET, "/e/x").first, kNone);
	EXPECT_EQ(at(b, GET, "/e/x/z").first, kNone);
}

TEST(RouteChains, APathThatEndsInsideAChainFindsNothing)
{
	const Built b = route_test::table({{GET, "/a/b/c"}, {GET, "/z/{p}"}});
	EXPECT_EQ(root_chain(b, "a"), "/b/c");
	EXPECT_EQ(at(b, GET, "/a/b/c").first, 0u);
	for (const char* path : {"/a", "/a/", "/a/b", "/a/b/", "/a/b/c/", "/a/b/cd", "/a/bc", "/a/b/c/d", "/ab/c"})
	{
		SCOPED_TRACE(path);
		EXPECT_EQ(find(b.view(), GET, path).status, Status::NotFound);
	}
}

TEST(RouteChains, APathThatLeavesAChainTakesTheParameterBesideIt)
{
	const Built p = route_test::table({{GET, "/a/b/c"}, {GET, "/{x}/b/d"}});
	EXPECT_EQ(at(p, GET, "/a/b/c"), std::make_pair(0u, Values{}));
	EXPECT_EQ(at(p, GET, "/a/b/d"), std::make_pair(1u, Values{"a"}));
	EXPECT_EQ(at(p, GET, "/q/b/d"), std::make_pair(1u, Values{"q"}));
	EXPECT_EQ(at(p, GET, "/a/x/d").first, kNone);

	const Built r = route_test::table({{GET, "/a/b/c"}, {GET, "/{*rest}"}});
	EXPECT_EQ(at(r, GET, "/a/b/c"), std::make_pair(0u, Values{}));
	EXPECT_EQ(at(r, GET, "/a/b/x"), std::make_pair(1u, Values{"a/b/x"}));
	EXPECT_EQ(at(r, GET, "/a/b"), std::make_pair(1u, Values{"a/b"}));
	EXPECT_EQ(at(r, GET, "/a/b/c/"), std::make_pair(1u, Values{"a/b/c/"}));
}

TEST(RouteChains, ATrailingSlashAfterAChainIsStrict)
{
	const Built b = route_test::table({{GET, "/a/b"}, {GET, "/a/b/"}, {GET, "/z/{p}"}});
	EXPECT_EQ(root_chain(b, "a"), "/b");
	EXPECT_EQ(at(b, GET, "/a/b").first, 0u);
	EXPECT_EQ(at(b, GET, "/a/b/").first, 1u);
	EXPECT_EQ(at(b, GET, "/a/b//").first, kNone);
	EXPECT_EQ(at(b, GET, "/a").first, kNone);
	EXPECT_EQ(at(b, GET, "/a/").first, kNone);

	const Built s = route_test::table({{GET, "/a/b/"}, {GET, "/z/{p}"}});  // the chain holds the empty last segment
	EXPECT_EQ(root_chain(s, "a"), "/b/");
	EXPECT_EQ(at(s, GET, "/a/b/").first, 0u);
	EXPECT_EQ(at(s, GET, "/a/b").first, kNone);
	EXPECT_EQ(at(s, GET, "/a/b/c").first, kNone);
}

TEST(RouteChains, AChainEndsAtANodeWithARouteAndChildren)
{
	const Built b = route_test::table({{GET, "/a/b/c"}, {GET, "/a/b/c/d"}, {GET, "/a/b/c/{x}"}, {GET, "/z"}});
	EXPECT_EQ(root_chain(b, "a"), "/b/c");
	EXPECT_EQ(at(b, GET, "/a/b/c"), std::make_pair(0u, Values{}));
	EXPECT_EQ(at(b, GET, "/a/b/c/d"), std::make_pair(1u, Values{}));
	EXPECT_EQ(at(b, GET, "/a/b/c/e"), std::make_pair(2u, Values{"e"}));
	EXPECT_EQ(at(b, GET, "/a/b").first, kNone);
}

TEST(RouteChains, LabelsLongerThanEightBytesInAChain)
{
	const Built b = route_test::table({{GET, "/abcdefghijk/lmnopqrstuvw/xyz0123456789"}, {GET, "/z/{p}"}});
	EXPECT_EQ(root_chain(b, "abcdefghijk"), "/lmnopqrstuvw/xyz0123456789");
	EXPECT_EQ(at(b, GET, "/abcdefghijk/lmnopqrstuvw/xyz0123456789").first, 0u);
	for (const char* path : {"/abcdefghijk/lmnopqrstuvX/xyz0123456789", "/abcdefghijk/lmnopqrXtuvw/xyz0123456789",
	                         "/abcdefghijk/lmnopqrstuvw/xyz012345678X", "/abcdefghijk/lmnopqrstuvw/xyz012345678",
	                         "/abcdefghijk/lmnopqrstuvw/xyz01234567890", "/abcdefghijk/lmnopqrstuvw",
	                         "/abcdefghijk/lmnopqrstuvwxyz0123456789", "/abcdefghijX/lmnopqrstuvw/xyz0123456789"})
	{
		SCOPED_TRACE(path);
		EXPECT_EQ(find(b.view(), GET, path).status, Status::NotFound);
	}
}

TEST(RouteChains, BacktrackingThroughAChainUnderANodeWithTwoKindsOfChild)
{
	// The path leaves the chain: the walk goes on with the parameter at the chain's node.
	const Built left = route_test::table({{GET, "/a/b/c/d"}, {GET, "/a/{x}/c/e"}});
	EXPECT_EQ(at(left, GET, "/a/b/c/d"), std::make_pair(0u, Values{}));
	EXPECT_EQ(at(left, GET, "/a/b/c/e"), std::make_pair(1u, Values{"b"}));
	EXPECT_EQ(at(left, GET, "/a/b/c").first, kNone);
	// The chain matches, the walk fails below it, and comes back to the parameter.
	const Built below = route_test::table({{GET, "/a/b/c/{y}/z"}, {GET, "/a/{x}/c/q/w"}});
	EXPECT_EQ(at(below, GET, "/a/b/c/q/w"), std::make_pair(1u, Values{"b"}));
	EXPECT_EQ(at(below, GET, "/a/b/c/q/z"), std::make_pair(0u, Values{"q"}));
	EXPECT_EQ(at(below, GET, "/a/b/c/q/y").first, kNone);
}

TEST(RouteChains, MethodNotAllowedThroughAChain)
{
	const Built b = route_test::table({{POST, "/a/b/{p}"}, {GET, "/other/{q}"}});
	EXPECT_EQ(at(b, POST, "/a/b/1"), std::make_pair(0u, Values{"1"}));
	for (unsigned method : {GET, PUT})
	{
		const Match m = find(b.view(), method, "/a/b/1");
		EXPECT_EQ(m.status, Status::MethodNotAllowed);
		EXPECT_EQ(m.allowed, 1u << POST);
		EXPECT_EQ(m.count, 0u);
	}
	EXPECT_EQ(find(b.view(), GET, "/a/c/1").status, Status::NotFound);
}

namespace
{
	constexpr RouteSpec kChainRoutes[] = {
		{GET, "/api/v1/users/{id}", 0},
		{GET, "/api/v1/users/me", 1},
		{GET, "/health", 2},
		{GET, "/a/b/c/d", 3},
		{GET, "/a/{x}/c/e", 4},
		{POST, "/a/b/{p}", 5},
	};
	alignas(kPageAlign) constexpr auto kChained = make_static_table<kChainRoutes>();
	static_assert(kChained.find(GET, "/api/v1/users/7").route == 0);
	static_assert(kChained.find(GET, "/api/v1/users/me").route == 1);
	static_assert(kChained.find(GET, "/api/v1").status == Status::NotFound);
	static_assert(kChained.find(GET, "/a/b/c/e").route == 4);
	static_assert(kChained.find(PUT, "/a/b/1").status == Status::MethodNotAllowed);
}  // namespace

TEST(RouteChains, ACompileTimeTableHasTheSameChains)
{
	const Built b = build_table(kChainRoutes);
	ASSERT_EQ(b.error, BuildError::None);
	route_test::expect_same_contents(kChained.view(), b.view());
	const RuntimeTable t = make_runtime_table(kChainRoutes);
	route_test::expect_same_contents(t.view(), b.view());
	route_test::expect_same_page_offsets(t.view(), kChained.view());
}
