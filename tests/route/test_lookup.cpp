// The lookup's path semantics (docs/route-semantics.md) on tables built at run time.
#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include <matcher/route.hpp>

#include "route_test_util.hpp"

using namespace matcher::route;
using route_test::GET;
using route_test::POST;
using route_test::PUT;
using route_test::table;
using route_test::values;

TEST(RouteLookup, TheMostSpecificRouteWinsInEitherRegistrationOrder)
{
	for (int order = 0; order < 2; ++order)
	{
		std::vector<RouteSpec> specs = {{GET, "/a/b", 0}, {GET, "/a/{x}", 0}, {GET, "/a/{n:u64}", 0},
		                                {GET, "/a/{*rest}", 0}};
		if (order == 1)
		{
			std::reverse(specs.begin(), specs.end());
		}
		for (std::uint32_t i = 0; i < specs.size(); ++i)
		{
			specs[i].route = i;  // the ids table() gives them, in registration order
		}
		const Built b = table(specs);
		const auto route_of = [&](const char* pattern) {
			for (const auto& s : specs)
			{
				if (s.pattern == pattern)
				{
					return s.route;
				}
			}
			return kNone;
		};
		EXPECT_EQ(find(b.view(), GET, "/a/b").route, route_of("/a/b"));
		EXPECT_EQ(find(b.view(), GET, "/a/42").route, route_of("/a/{n:u64}"));
		EXPECT_EQ(find(b.view(), GET, "/a/c").route, route_of("/a/{x}"));
		EXPECT_EQ(find(b.view(), GET, "/a/c/d").route, route_of("/a/{*rest}"));
	}
}

TEST(RouteLookup, AFailedLiteralBranchBacktracksToTheParameter)
{
	const Built b = table({{GET, "/a/b/c", 0}, {GET, "/a/{x}/d", 0}, {GET, "/a/{x}/{*r}", 0}});
	EXPECT_EQ(find(b.view(), GET, "/a/b/c").route, 0u);
	const Match m = find(b.view(), GET, "/a/b/d");
	EXPECT_EQ(m.route, 1u);
	EXPECT_EQ(values(m), std::vector<std::string>{"b"});
	const Match r = find(b.view(), GET, "/a/b/e/f");
	EXPECT_EQ(r.route, 2u);
	EXPECT_EQ(values(r), (std::vector<std::string>{"b", "e/f"}));
}

TEST(RouteLookup, AParameterAcceptsEveryPcharTakenAsSent)
{
	const Built b = table({{GET, "/u/{x}", 0}});
	for (const char* v : {"a-b", "a.b", "a_b", "a~b", "a!b", "a$b", "a&b", "a'b", "a(b)", "a*b", "a+b", "a,b", "a;b",
	                      "a=b", "a:b", "a@b", "a%20b", "a%2Fb"})
	{
		SCOPED_TRACE(v);
		const std::string path = std::string("/u/") + v;  // the values are views into it
		const Match m = find(b.view(), GET, path);
		EXPECT_EQ(m.route, 0u);
		EXPECT_EQ(values(m), std::vector<std::string>{v});
	}
}

TEST(RouteLookup, LiteralsAreComparedAsRawBytes)
{
	const Built b = table({{GET, "/caf%C3%A9", 0}, {GET, "/x/caf%C3%A9/{y}", 0}});
	EXPECT_EQ(find(b.view(), GET, "/caf%C3%A9").route, 0u);
	EXPECT_EQ(find(b.view(), GET, "/caf%c3%a9").status, Status::NotFound);
	EXPECT_EQ(find(b.view(), GET, "/x/caf%C3%A9/z").route, 1u);
	EXPECT_EQ(find(b.view(), GET, "/x/caf%c3%a9/z").status, Status::NotFound);
}

TEST(RouteLookup, AnEmptySegmentNeverMatchesAParameter)
{
	const Built b = table({{GET, "/u/{x}", 0}, {GET, "/v/{x}/w", 0}});
	EXPECT_EQ(find(b.view(), GET, "/u/").status, Status::NotFound);
	EXPECT_EQ(find(b.view(), GET, "/u//").status, Status::NotFound);
	EXPECT_EQ(find(b.view(), GET, "/v//w").status, Status::NotFound);
}

TEST(RouteLookup, ATrailingSlashIsStrict)
{
	const Built b = table({{GET, "/a/b", 0}, {GET, "/c/{x}", 0}, {GET, "/d/", 0}, {GET, "/e/{x}/", 0}});
	EXPECT_EQ(find(b.view(), GET, "/a/b").route, 0u);
	EXPECT_EQ(find(b.view(), GET, "/a/b/").status, Status::NotFound);
	EXPECT_EQ(find(b.view(), GET, "/c/x/").status, Status::NotFound);
	EXPECT_EQ(find(b.view(), GET, "/d/").route, 2u);
	EXPECT_EQ(find(b.view(), GET, "/d").status, Status::NotFound);
	EXPECT_EQ(find(b.view(), GET, "/e/x/").route, 3u);
	EXPECT_EQ(find(b.view(), GET, "/e/x").status, Status::NotFound);
}

TEST(RouteLookup, ACatchAllMatchesANonEmptyRemainderTakenAsSent)
{
	const Built b = table({{GET, "/f/{*p}", 0}});
	EXPECT_EQ(values(find(b.view(), GET, "/f/a")), std::vector<std::string>{"a"});
	EXPECT_EQ(values(find(b.view(), GET, "/f/a/b/c")), std::vector<std::string>{"a/b/c"});
	EXPECT_EQ(values(find(b.view(), GET, "/f/a/")), std::vector<std::string>{"a/"});
	EXPECT_EQ(values(find(b.view(), GET, "/f/a//b")), std::vector<std::string>{"a//b"});
	EXPECT_EQ(find(b.view(), GET, "/f").status, Status::NotFound);
	EXPECT_EQ(find(b.view(), GET, "/f/").status, Status::NotFound);
	std::string deep = "/f";
	for (int i = 0; i < 100; ++i)
	{
		deep += "/s";
	}
	EXPECT_EQ(find(b.view(), GET, deep).route, 0u);
}

TEST(RouteLookup, APathThatMatchesOnlyUnderAnotherMethodIs405)
{
	const Built b = table({{GET, "/a/b", 0}, {POST, "/a/{x}", 0}, {GET, "/p/{x}", 0}, {PUT, "/p/{x}", 0}});
	EXPECT_EQ(find(b.view(), GET, "/a/b").route, 0u);
	EXPECT_EQ(find(b.view(), POST, "/a/b").route, 1u);  // POST's own most specific route first
	const Match m = find(b.view(), POST, "/p/q");
	EXPECT_EQ(m.status, Status::MethodNotAllowed);
	EXPECT_EQ(m.allowed, (1u << GET) | (1u << PUT));
	EXPECT_EQ(m.count, 0);
	EXPECT_EQ(find(b.view(), PUT, "/a/b").status, Status::MethodNotAllowed);
	EXPECT_EQ(find(b.view(), GET, "/zz").status, Status::NotFound);
}

TEST(RouteLookup, TypedParametersTakePartInRouting)
{
	const Built b = table({{GET, "/users/{id:u64}", 0}, {GET, "/users/me", 0}, {GET, "/users/{name}", 0},
	                       {GET, "/t/{n:i64}", 0}});
	EXPECT_EQ(find(b.view(), GET, "/users/42").route, 0u);
	EXPECT_EQ(find(b.view(), GET, "/users/18446744073709551615").route, 0u);
	EXPECT_EQ(find(b.view(), GET, "/users/18446744073709551616").route, 2u);  // does not fit in u64
	EXPECT_EQ(find(b.view(), GET, "/users/me").route, 1u);
	EXPECT_EQ(find(b.view(), GET, "/users/bob").route, 2u);
	EXPECT_EQ(find(b.view(), GET, "/t/-9223372036854775808").route, 3u);
	EXPECT_EQ(find(b.view(), GET, "/t/-9223372036854775809").status, Status::NotFound);
	EXPECT_EQ(find(b.view(), GET, "/t/x").status, Status::NotFound);
}

TEST(RouteLookup, CapturedValuesAreViewsIntoThePath)
{
	const Built b = table({{GET, "/a/{x}/b/{y}", 0}});
	const std::string path = "/a/first/b/second";
	const Match m = find(b.view(), GET, path);
	ASSERT_EQ(m.count, 2);
	EXPECT_EQ(m.params[0].data(), path.data() + 3);
	EXPECT_EQ(m.params[1].data(), path.data() + 11);
}

TEST(RouteLookup, WideNodesLongLabelsAndLabelsSharingTheirFirst8Bytes)
{
	std::vector<std::string> patterns;
	for (int i = 0; i < 300; ++i)
	{
		patterns.push_back("/wide/label-" + std::to_string(i) + "-shared-prefix/{x}");
		patterns.push_back("/wide/{y}/end" + std::to_string(i));
	}
	patterns.push_back("/wide/abcdefgh/{x}");
	patterns.push_back("/wide/abcdefghi/{x}");
	patterns.push_back("/wide/abcdefgh12345678/{x}");
	std::vector<RouteSpec> specs;
	for (const auto& p : patterns)
	{
		specs.push_back({GET, p, 0});
	}
	const Built b = table(specs);
	for (std::uint32_t i = 0; i < patterns.size(); ++i)
	{
		std::string path = patterns[i];
		const auto brace = path.find("{x}");
		if (brace != std::string::npos)
		{
			path.replace(brace, 3, "v");
		}
		const auto y = path.find("{y}");
		if (y != std::string::npos)
		{
			path.replace(y, 3, "not-a-label");
		}
		SCOPED_TRACE(path);
		EXPECT_EQ(find(b.view(), GET, path).route, i);
	}
}

TEST(RouteLookup, FindIntoFillsAReusedMatchAsFindReturnsAFreshOne)
{
	const Built b = table({{GET, "/a/{x}/c", 0}, {GET, "/a/b", 0}, {POST, "/p/{y}", 0}});
	const TableView v = b.view();
	Match m;
	for (int pass = 0; pass < 2; ++pass)
	{
		for (const auto& [method, path] : std::vector<std::pair<unsigned, std::string>>{
		         {GET, "/a/q/c"}, {GET, "/a/b"}, {GET, "/p/z"}, {GET, "/none"}, {POST, "/p/z"}, {PUT, "/a/q/c"}})
		{
			find_into(v, method, path, m);
			const Match f = find(v, method, path);
			EXPECT_EQ(m.status, f.status);
			EXPECT_EQ(m.route, f.route);
			EXPECT_EQ(m.allowed, f.allowed);
			EXPECT_EQ(values(m), values(f));
		}
	}
}
