// Helpers shared by the route matcher's tests.
#pragma once

#include <gtest/gtest.h>

#include <cstddef>
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

	// Two views hold the same table: every field of every node, edge and exact-match slot, the
	// arena, the roots and the method sets, compared field by field so that padding never counts.
	inline void expect_same_contents(const matcher::route::TableView& a, const matcher::route::TableView& b)
	{
		ASSERT_EQ(a.nodes.size(), b.nodes.size());
		for (std::size_t i = 0; i < a.nodes.size(); ++i)
		{
			const auto& x = a.nodes[i];
			const auto& y = b.nodes[i];
			EXPECT_TRUE(x.edges == y.edges && x.count == y.count && x.hashed == y.hashed && x.branches == y.branches &&
			            x.chain == y.chain && x.u64 == y.u64 && x.i64 == y.i64 && x.param == y.param && x.rest == y.rest &&
			            x.route == y.route)
				<< "node " << i;
		}
		ASSERT_EQ(a.edges.size(), b.edges.size());
		for (std::size_t i = 0; i < a.edges.size(); ++i)
		{
			const auto& x = a.edges[i];
			const auto& y = b.edges[i];
			EXPECT_TRUE(x.word == y.word && x.child == y.child && x.off == y.off && x.len == y.len) << "edge " << i;
		}
		ASSERT_EQ(a.literals.size(), b.literals.size());
		for (std::size_t i = 0; i < a.literals.size(); ++i)
		{
			const auto& x = a.literals[i];
			const auto& y = b.literals[i];
			EXPECT_TRUE(x.hash == y.hash && x.off == y.off && x.len == y.len && x.route == y.route && x.method == y.method)
				<< "exact-match slot " << i;
		}
		EXPECT_EQ(a.arena, b.arena);
		EXPECT_EQ(a.roots, b.roots);
		EXPECT_EQ(a.methods, b.methods);
		EXPECT_EQ(a.literal_methods, b.literal_methods);
	}

	inline std::size_t page_offset(const void* p)
	{
		return reinterpret_cast<std::uintptr_t>(p) % matcher::route::kPageAlign;
	}

	// Every non-empty array of the two views starts at the same offset into a page. (An empty
	// array is never read; where its pointer points is the standard library's choice.)
	inline void expect_same_page_offsets(const matcher::route::TableView& a, const matcher::route::TableView& b)
	{
		ASSERT_EQ(a.nodes.empty(), b.nodes.empty());
		ASSERT_EQ(a.edges.empty(), b.edges.empty());
		ASSERT_EQ(a.literals.empty(), b.literals.empty());
		if (!a.nodes.empty())
		{
			EXPECT_EQ(page_offset(a.nodes.data()), page_offset(b.nodes.data())) << "nodes";
		}
		if (!a.edges.empty())
		{
			EXPECT_EQ(page_offset(a.edges.data()), page_offset(b.edges.data())) << "edges";
		}
		if (!a.literals.empty())
		{
			EXPECT_EQ(page_offset(a.literals.data()), page_offset(b.literals.data())) << "exact-match slots";
		}
		EXPECT_EQ(page_offset(a.arena.data()), page_offset(b.arena.data())) << "arena";
	}
}  // namespace route_test
