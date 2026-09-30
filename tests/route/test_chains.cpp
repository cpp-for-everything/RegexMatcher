// Chain nodes: a node that has no route and one literal child only, and the nodes below it that
// are the same, become one node of the kind kNodeChain when they are at least two such segments
// (one alone saves no step). It has no edges and no parameter
// children; its segments are one run of bytes in the arena (Node::edges: where, Node::chain: how
// many), and its child is the next node. The walk tests a chain only where a node found nothing,
// and answers exactly as over the trie without chains: every answer below is the one the
// routes' semantics give.
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
	// The edge of node n whose label is `first`, or null.
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

	// A chain node's segments, or a note that the node is not one.
	std::string chain_of(const TableView& v, std::uint32_t node)
	{
		const Node& n = v.nodes[node];
		if (n.hashed != kNodeChain)
		{
			return "not a chain";
		}
		return std::string(v.arena.substr(n.edges, n.chain));
	}

	// The chain below the root's edge `first` for method GET (a method whose routes are all
	// literal has no trie, so every table below has a parameter route too).
	std::string chain_below(const Built& b, std::string_view first)
	{
		const TableView v = b.view();
		if (v.roots[GET] == kNone)
		{
			return "no trie";
		}
		const Edge* e = edge_to(v, v.nodes[v.roots[GET]], first);
		return e == nullptr ? "no edge" : chain_of(v, e->child);
	}

	// The route a lookup finds (kNone: none) and its captured values.
	std::pair<std::uint32_t, std::vector<std::string>> at(const Built& b, unsigned method, std::string_view path)
	{
		const Match m = find(b.view(), method, path);
		return {m.status == Status::Found ? m.route : kNone, values(m)};
	}

	using Values = std::vector<std::string>;
}  // namespace

TEST(RouteChains, ARunOfSingleChildLiteralNodesIsOneNode)
{
	const Built b = route_test::table({{GET, "/api/v1/users/{id}"}, {GET, "/api/v1/users/me"}, {GET, "/health"}});
	const TableView v = b.view();
	const Node& root = v.nodes[v.roots[GET]];
	EXPECT_EQ(root.hashed, kNodeLinear);
	const Edge* api = edge_to(v, root, "api");
	ASSERT_NE(api, nullptr);
	EXPECT_EQ(chain_of(v, api->child), "v1/users");
	const Node& chain = v.nodes[api->child];
	EXPECT_EQ(chain.count, 0u);
	EXPECT_EQ(chain.route, kNone);
	EXPECT_EQ(chain.u64 & chain.i64 & chain.param & chain.rest, kNone);
	const Node& users = v.nodes[api->child + 1];  // the chain's child is the next node
	EXPECT_NE(users.param, kNone);
	EXPECT_EQ(users.count, 1u);
	EXPECT_EQ(chain_below(b, "health"), "not a chain");
	EXPECT_EQ(v.nodes.size(), 6u);  // the root, the chain, users, me, {id}, health
	EXPECT_EQ(at(b, GET, "/api/v1/users/7"), std::make_pair(0u, Values{"7"}));
	EXPECT_EQ(at(b, GET, "/api/v1/users/me"), std::make_pair(1u, Values{}));
	EXPECT_EQ(at(b, GET, "/health"), std::make_pair(2u, Values{}));
}

TEST(RouteChains, ANodeBelowAHashedNodeChainsToo)
{
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
	for (char c = 'a'; c < 'a' + 9; ++c)
	{
		EXPECT_EQ(chain_below(b, std::string(1, c)), "x/y");
	}
	EXPECT_EQ(chain_below(b, "zz"), "not a chain");
	EXPECT_EQ(at(b, GET, "/e/x/y").first, 4u);
	EXPECT_EQ(at(b, GET, "/e/x").first, kNone);
	EXPECT_EQ(at(b, GET, "/e/x/z").first, kNone);
	EXPECT_EQ(at(b, GET, "/zz/1"), std::make_pair(9u, Values{"1"}));
}

TEST(RouteChains, AMethodsRootCanBeAChain)
{
	const Built b = route_test::table({{GET, "/api/v1/{x}"}, {GET, "/api/v1/{x}/y"}});
	const TableView v = b.view();
	EXPECT_EQ(chain_of(v, v.roots[GET]), "api/v1");
	EXPECT_EQ(at(b, GET, "/api/v1/7"), std::make_pair(0u, Values{"7"}));
	EXPECT_EQ(at(b, GET, "/api/v1/7/y"), std::make_pair(1u, Values{"7"}));
	for (const char* path : {"/", "/api", "/api/", "/api/v1", "/api/v1/", "/api/v2/7", "/apx/v1/7", "/api/v1x/7"})
	{
		SCOPED_TRACE(path);
		EXPECT_EQ(find(b.view(), GET, path).status, Status::NotFound);
	}
}

TEST(RouteChains, ARunOfOneSegmentIsNotAChain)
{
	const Built b = route_test::table({{GET, "/a/b/{p}"}, {GET, "/z/{q}"}});
	EXPECT_EQ(chain_below(b, "a"), "not a chain");
	EXPECT_EQ(at(b, GET, "/a/b/1"), std::make_pair(0u, Values{"1"}));
	const Built c = route_test::table({{GET, "/a/b/c/{p}"}, {GET, "/z/{q}"}});
	EXPECT_EQ(chain_below(c, "a"), "b/c");
	EXPECT_EQ(at(c, GET, "/a/b/c/1"), std::make_pair(0u, Values{"1"}));
}

TEST(RouteChains, APathThatEndsInsideAChainFindsNothing)
{
	const Built b = route_test::table({{GET, "/a/b/c"}, {GET, "/z/{p}"}});
	EXPECT_EQ(chain_below(b, "a"), "b/c");
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
	EXPECT_EQ(chain_below(p, "a"), "b/c");
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
	EXPECT_EQ(chain_below(b, "a"), "not a chain");  // one segment, "b", then a node with a route
	EXPECT_EQ(at(b, GET, "/a/b").first, 0u);
	EXPECT_EQ(at(b, GET, "/a/b/").first, 1u);
	EXPECT_EQ(at(b, GET, "/a/b//").first, kNone);
	EXPECT_EQ(at(b, GET, "/a").first, kNone);
	EXPECT_EQ(at(b, GET, "/a/").first, kNone);

	const Built s = route_test::table({{GET, "/a/b/"}, {GET, "/z/{p}"}});  // the chain holds the empty last segment
	EXPECT_EQ(chain_below(s, "a"), "b/");
	EXPECT_EQ(at(s, GET, "/a/b/").first, 0u);
	EXPECT_EQ(at(s, GET, "/a/b").first, kNone);
	EXPECT_EQ(at(s, GET, "/a/b/c").first, kNone);
	EXPECT_EQ(at(s, GET, "/a/b//").first, kNone);
}

TEST(RouteChains, AChainEndsAtANodeWithARouteAndChildren)
{
	const Built b = route_test::table({{GET, "/a/b/c"}, {GET, "/a/b/c/d"}, {GET, "/a/b/c/{x}"}, {GET, "/z"}});
	EXPECT_EQ(chain_below(b, "a"), "b/c");
	EXPECT_EQ(at(b, GET, "/a/b/c"), std::make_pair(0u, Values{}));
	EXPECT_EQ(at(b, GET, "/a/b/c/d"), std::make_pair(1u, Values{}));
	EXPECT_EQ(at(b, GET, "/a/b/c/e"), std::make_pair(2u, Values{"e"}));
	EXPECT_EQ(at(b, GET, "/a/b").first, kNone);
}

TEST(RouteChains, LabelsLongerThanEightBytesInAChain)
{
	const Built b = route_test::table({{GET, "/abcdefghijk/lmnopqrstuvw/xyz0123456789"}, {GET, "/z/{p}"}});
	EXPECT_EQ(chain_below(b, "abcdefghijk"), "lmnopqrstuvw/xyz0123456789");
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

TEST(RouteChains, AChainLongerThanItsLengthFieldIsSplit)
{
	// 30 segments of 3,000 bytes, then a parameter: below the first segment, a chain of 29
	// segments, 87,028 bytes, more than Node::chain holds (65,535).
	std::string pattern;
	for (int i = 0; i < 30; ++i)
	{
		pattern += '/';
		pattern += std::string(3000, static_cast<char>('a' + i % 26));
	}
	std::string path = pattern;
	pattern += "/{p}";
	path += "/7";
	const Built b = route_test::table({{GET, pattern}, {GET, "/z/{q}"}});
	const TableView v = b.view();
	std::size_t chains = 0;
	std::size_t bytes = 0;
	for (std::uint32_t i = 0; i < v.nodes.size(); ++i)
	{
		if (v.nodes[i].hashed == kNodeChain)
		{
			++chains;
			bytes += v.nodes[i].chain;
		}
	}
	EXPECT_GE(chains, 2u);
	EXPECT_EQ(at(b, GET, path), std::make_pair(0u, Values{"7"}));
	std::string wrong = path;
	wrong[wrong.size() - 5] = '#';
	EXPECT_EQ(find(b.view(), GET, wrong).status, Status::NotFound);
	EXPECT_GT(bytes, 0u);
}

TEST(RouteChains, BacktrackingThroughAChainUnderANodeWithTwoKindsOfChild)
{
	// The path leaves the chain: the walk goes on with the parameter at the chain's parent.
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
