#include "utils/test.hpp"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>

TEST(RegexMatcherValidity, matcher_urls) { test_for({"/", "\\/([0-9a-z\\-]+)"}, {"/", "/12asdf-"}); }

TEST(RegexMatcherValidity, matcher_test_no_regex) {
	test_for({"text1", "text2", "text3"}, {"text", "text1", "text2", "text3", "text4"});
}

TEST(RegexMatcherValidity, matcher_test_group) {
	test_for({"text(1)", "text(2)", "text(3)"}, {"text", "text1", "text2", "text3", "text4"});
}

TEST(RegexMatcherValidity, matcher_test_global_or) {
	test_for({"text1|test2|test3"}, {"text", "text1", "text2", "text3", "text4"});
}

TEST(RegexMatcherValidity, matcher_test_or_within_group) {
	test_for({"text(1|2|3)"}, {"text", "text1", "text2", "text3", "text4"});
}

TEST(RegexMatcherValidity, matcher_test_letter_asterix) {
	test_for({"text1*"}, {"text", "text1", "text2", "text3", "text4"});
}

TEST(RegexMatcherValidity, matcher_test_group_asterix) {
	test_for({"text(1)*"}, {"text", "text1", "text2", "text3", "text4"});
}

TEST(RegexMatcherValidity, matcher_test_group_with_or_asterix) {
	test_for({"text(1|2|3)*"}, {"text", "text1", "text2", "text3", "text4"});
}

TEST(RegexMatcherValidity, matcher_test_group_with_or_plus) {
	test_for({"text(1|2|3)+"}, {"text", "text1", "text21", "text31", "text34"});
}

TEST(RegexMatcherValidity, matcher_test_group_with_repeat) {
	test_for({"text(1|2|3){1,2}"}, {"text", "text1", "text21", "text31", "text34", "text123123"});
}

TEST(RegexMatcherValidity, matcher_test_group_with_question_mark) {
	test_for({"text(1|2|3)?"}, {"text", "text1", "text21", "text31", "text34", "text123123"});
}

// A character class is one node carrying its members rather than one node per member.
// That changes how classes are built, shared and looked up, so these press on the parts
// where it could go wrong: two classes leaving the same node, classes that overlap each
// other and the literals around them, and repetition over a class. Every case is checked
// against std::regex by test_for, so none of them encodes what this library happens to do.

TEST(RegexMatcherValidity, matcher_class_two_different_classes_from_one_node) {
	// Both leave the node for 'a'. Keyed by the class rather than by the character read,
	// so a single class slot per node would have merged these into one.
	test_for({"a[0-9]x", "a[a-z]y"}, {"a1x", "aqy", "a1y", "aqx", "ax", "ay", "a1", "aq"});
}

TEST(RegexMatcherValidity, matcher_class_same_class_twice_from_one_node) {
	// Identical members under the same parent: the two patterns should share the state.
	test_for({"a[0-9]x", "a[0-9]y"}, {"a1x", "a1y", "a9x", "a9y", "ax", "a1z"});
}

TEST(RegexMatcherValidity, matcher_class_overlapping_members) {
	test_for({"[a-c]z", "[b-d]z"}, {"az", "bz", "cz", "dz", "ez", "z", "bb"});
}

TEST(RegexMatcherValidity, matcher_class_overlaps_a_literal_edge) {
	// 'b' is reachable both as a literal and as a class member from the same node.
	test_for({"xbz", "x[a-c]z"}, {"xaz", "xbz", "xcz", "xdz", "xz"});
}

TEST(RegexMatcherValidity, matcher_class_with_repetition) {
	test_for({"a[0-9]+b", "a[0-9]{2,3}c"}, {"a1b", "a123b", "ab", "a12c", "a123c", "a1234c"});
}

// Repeat lower bounds, which went unenforced in two separate ways.
//
// A bound was only ever checked where a pattern ends, so a repeat that exited into more
// pattern was never asked whether it had run its minimum, and a[0-9]{2,3}c matched a1c.
// Separately, {n} with no comma set the upper bound and left the lower one at zero, so
// {3} meant "up to three" and a[0-9]{3}z matched az.

TEST(RegexMatcherValidity, matcher_repeat_bound_at_end_of_pattern) {
	test_for({"a[0-9]{2,3}"}, {"a", "a1", "a12", "a123", "a1234"});
}

TEST(RegexMatcherValidity, matcher_repeat_bound_followed_by_more_pattern) {
	test_for({"a[0-9]{2,3}c"}, {"ac", "a1c", "a12c", "a123c", "a1234c"});
}

TEST(RegexMatcherValidity, matcher_repeat_exact_count) {
	test_for({"a[0-9]{3}z"}, {"az", "a1z", "a12z", "a123z", "a1234z"});
}

TEST(RegexMatcherValidity, matcher_repeat_open_upper_bound) {
	test_for({"a[0-9]{2,}z"}, {"az", "a1z", "a12z", "a12345z"});
}

TEST(RegexMatcherValidity, matcher_repeat_zero_lower_bound_stays_permissive) {
	test_for({"a[0-9]{0,2}z", "a[0-9]*z", "a[0-9]+z"}, {"az", "a1z", "a12z", "a123z"});
}

TEST(RegexMatcherValidity, matcher_repeat_over_a_literal) {
	test_for({"ab{2,3}c"}, {"ac", "abc", "abbc", "abbbc", "abbbbc"});
}

TEST(RegexMatcherValidity, matcher_two_bounded_repeats_in_sequence) {
	test_for({"a[0-9]{2}b[a-z]{2}c"}, {"a12bxyc", "a1bxyc", "a12bxc", "a12bxyzc"});
}

TEST(RegexMatcherValidity, matcher_repeat_bound_inside_a_group) {
	test_for({"\\/x\\/([0-9]{2,3})\\/y"}, {"/x/1/y", "/x/12/y", "/x/123/y", "/x/1234/y"});
}

TEST(RegexMatcherValidity, matcher_class_single_member_and_leading) {
	test_for({"[a]bc", "[0-9]start"}, {"abc", "bbc", "1start", "start", "abcd"});
}

TEST(RegexMatcherValidity, matcher_class_inside_alternation_and_group) {
	test_for({"([0-9]|[a-c])end", "p([x-z]+)q"}, {"1end", "bend", "dend", "end", "pxq", "pxyzq", "pq", "paq"});
}

TEST(RegexMatcherValidity, matcher_class_route_shaped) {
	// The shape the router generates: escaped slashes, a capture over a class, a repeat.
	test_for({"\\/api\\/v1\\/([A-Za-z0-9_.%\\-]+)", "\\/api\\/v1\\/fixed"},
	         {"/api/v1/abc", "/api/v1/fixed", "/api/v1/", "/api/v1/a-b.c", "/api/v1/a/b", "/api/v2/abc"});
}

TEST(RegexMatcherValidity, matcher_tests_many_regexes_many_matches) {
	test_for(
	    {"d(abc|def)*g+", "d(abc)*g+", "a?", "b|c", "(d|e)f", "f[a-c]?d(ab|cd)*g+", "a{1,3}a", "aaa", "aa"},
	    {"a",         "aa",        "aaa",       "aaaa",       "a",          "b",          "c",         "d",
	     "df",        "e",         "ef",        "fdg",        "fdgg",       "fd",         "fdabgg",    "fdababgg",
	     "fdabcdgg",  "fdcdcdgg",  "fdacgg",    "fadabgg",    "fadababgg",  "fadabcdgg",  "fadcdcdgg", "fadacgg",
	     "fbdabgg",   "fbdababgg", "fbdabcdgg", "fbdcdcdgg",  "fbdacgg",    "fcdabgg",    "fcdababgg", "fcdabcdgg",
	     "fcdcdcdgg", "fcdacgg",   "fccdabgg",  "fccdababgg", "fccdabcdgg", "fccdcdcdgg", "fccdacgg",  "dab",
	     "dabcabc",   "dabc",      "ddefdef",   "dabcg",      "dg",         "dabcabcg",   "ddefabcg",  "ddefdefg"});
}

// Tests for group capture functionality
TEST(RegexMatcherGroups, simple_group_capture) {
	matcher::RegexMatcher<int, char> root;
	root.add_regex(std::string("a(bc)d"), 0);

	auto results = root.match_with_groups(std::string("abcd"));
	ASSERT_EQ(results.size(), 1);
	EXPECT_EQ(results[0].regex_id, 0);
	ASSERT_EQ(results[0].groups.size(), 1);
	// Group 0 should capture "bc" at positions 1-3
	EXPECT_EQ(results[0].groups.at(0).first, 1);
	EXPECT_EQ(results[0].groups.at(0).second, 3);
}

TEST(RegexMatcherGroups, multiple_groups) {
	matcher::RegexMatcher<int, char> root;
	root.add_regex(std::string("(a)(b)(c)"), 0);

	auto results = root.match_with_groups(std::string("abc"));
	ASSERT_EQ(results.size(), 1);
	EXPECT_EQ(results[0].regex_id, 0);
	ASSERT_EQ(results[0].groups.size(), 3);
	// Group 0: "a" at 0-1
	EXPECT_EQ(results[0].groups.at(0).first, 0);
	EXPECT_EQ(results[0].groups.at(0).second, 1);
	// Group 1: "b" at 1-2
	EXPECT_EQ(results[0].groups.at(1).first, 1);
	EXPECT_EQ(results[0].groups.at(1).second, 2);
	// Group 2: "c" at 2-3
	EXPECT_EQ(results[0].groups.at(2).first, 2);
	EXPECT_EQ(results[0].groups.at(2).second, 3);
}

TEST(RegexMatcherGroups, nested_groups) {
	matcher::RegexMatcher<int, char> root;
	root.add_regex(std::string("a((b)c)d"), 0);

	auto results = root.match_with_groups(std::string("abcd"));
	ASSERT_EQ(results.size(), 1);
	EXPECT_EQ(results[0].regex_id, 0);
	ASSERT_EQ(results[0].groups.size(), 2);
	// Group 0: "bc" at 1-3
	EXPECT_EQ(results[0].groups.at(0).first, 1);
	EXPECT_EQ(results[0].groups.at(0).second, 3);
	// Group 1: "b" at 1-2
	EXPECT_EQ(results[0].groups.at(1).first, 1);
	EXPECT_EQ(results[0].groups.at(1).second, 2);
}

TEST(RegexMatcherGroups, group_with_alternation) {
	matcher::RegexMatcher<int, char> root;
	root.add_regex(std::string("a(b|c)d"), 0);

	auto results1 = root.match_with_groups(std::string("abd"));
	ASSERT_EQ(results1.size(), 1);
	EXPECT_EQ(results1[0].groups.at(0).first, 1);
	EXPECT_EQ(results1[0].groups.at(0).second, 2);

	auto results2 = root.match_with_groups(std::string("acd"));
	ASSERT_EQ(results2.size(), 1);
	// Check if group 0 exists before accessing
	EXPECT_TRUE(results2[0].groups.find(0) != results2[0].groups.end());
	if (results2[0].groups.find(0) != results2[0].groups.end()) {
		EXPECT_EQ(results2[0].groups.at(0).first, 1);
		EXPECT_EQ(results2[0].groups.at(0).second, 2);
	}
}

TEST(RegexMatcherGroups, group_with_repetition) {
	matcher::RegexMatcher<int, char> root;
	root.add_regex(std::string("a(bc)+d"), 0);

	auto results = root.match_with_groups(std::string("abcbcd"));
	ASSERT_EQ(results.size(), 1);
	// The group should capture the last repetition or full span depending on semantics
	EXPECT_TRUE(results[0].groups.find(0) != results[0].groups.end());
}

TEST(RegexMatcherGroups, no_match_no_groups) {
	matcher::RegexMatcher<int, char> root;
	root.add_regex(std::string("a(bc)d"), 0);

	auto results = root.match_with_groups(std::string("xyz"));
	EXPECT_EQ(results.size(), 0);
}

TEST(RegexMatcherGroups, multiple_regexes_with_groups) {
	matcher::RegexMatcher<int, char> root;
	root.add_regex(std::string("(a)bc"), 0);
	root.add_regex(std::string("a(b)c"), 1);

	auto results = root.match_with_groups(std::string("abc"));
	ASSERT_EQ(results.size(), 2);
	// Both regexes should match with their respective groups
}

// A matcher is shared. A router can hand one RegexMatcher to every worker thread and call
// match_with_groups from all of them. Matching used to decrement the Limits counter
// behind each quantifier inside the shared graph and restore it on the way out, so two
// threads raced on the same counters.
//
// The routes have the shape such a router builds, where "/user/{id}" becomes
// "\/user\/([A-Za-z0-9_.%\-]+)". An unbounded quantifier lands on Limits{0, none}, which
// the decrement leaves alone, so the race there only ever writes back the value it read.
// The bounded repeats are the counters that move: without the fix about a third of the
// matches below come back empty, because a counter was read while another thread was
// part way through restoring it.
TEST(RegexMatcherConcurrency, match_with_groups_over_a_route_table) {
	struct Route {
		std::string pattern;
		std::string path;
		std::string expected;  // "<route id>|<param>|<param>..."
	};
	const std::vector<Route> routes = {
	    {R"(\/health)", "/health", "0"},
	    {R"(\/user\/([A-Za-z0-9_.%\-]+))", "/user/alice", "1|alice"},
	    {R"(\/user\/([A-Za-z0-9_.%\-]+)\/post\/([A-Za-z0-9_.%\-]+))", "/user/bob/post/42", "2|bob|42"},
	    {R"(\/orders\/([A-Za-z0-9_.%\-]+))", "/orders/9f2c", "3|9f2c"},
	    {R"(\/logs\/([0-9]{4}))", "/logs/2026", "4|2026"},
	    {R"(\/api\/v([0-9]{1,2})\/items\/([A-Za-z0-9_.%\-]+))", "/api/v12/items/x.y", "5|12|x.y"},
	};

	matcher::RegexMatcher<size_t> root;
	for (size_t i = 0; i < routes.size(); i++) {
		root.add_regex(routes[i].pattern, i);
	}

	// Router takes the last match, so describe that one as "<route id>|<param>...".
	// A match resolved to the wrong route and a wrong capture both show up as a mismatch.
	auto resolve = [&root, &routes](size_t i) {
		const std::string& path = routes[i].path;
		const auto results = root.match_with_groups(path);
		if (results.empty()) {
			return std::string("<no match>");
		}
		const auto& match = results.back();
		std::string answer = std::to_string(match.regex_id);
		for (const auto& group : match.groups) {
			answer += "|" + path.substr(group.second.first, group.second.second - group.second.first);
		}
		return answer;
	};

	// Uncontended, every path resolves to its own route.
	for (size_t i = 0; i < routes.size(); i++) {
		ASSERT_EQ(resolve(i), routes[i].expected) << "route table is wrong before any thread starts";
	}

	const unsigned workers = std::max(4u, std::min(8u, std::thread::hardware_concurrency()));
	constexpr size_t rounds = 500;

	std::atomic<bool> go{false};
	std::atomic<size_t> mismatches{0};
	std::mutex first_failure_lock;
	std::string first_failure;

	std::vector<std::thread> threads;
	for (unsigned worker = 0; worker < workers; worker++) {
		threads.emplace_back([&] {
			while (!go.load(std::memory_order_acquire)) {
			}
			for (size_t round = 0; round < rounds; round++) {
				for (size_t i = 0; i < routes.size(); i++) {
					const std::string got = resolve(i);
					if (got != routes[i].expected) {
						mismatches.fetch_add(1, std::memory_order_relaxed);
						const std::lock_guard<std::mutex> guard(first_failure_lock);
						if (first_failure.empty()) {
							first_failure = routes[i].path + " -> " + got + ", expected " + routes[i].expected;
						}
					}
				}
			}
		});
	}
	go.store(true, std::memory_order_release);
	for (auto& thread : threads) {
		thread.join();
	}

	EXPECT_EQ(mismatches.load(), 0u) << "first failure: " << first_failure;
}

// Performance benchmarks have been moved to benchmarks.cpp using Google Benchmark
// Run: ./build/tests/benchmarks.exe

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

int main(int argc, char** argv) {
	std::cout << "RegexMatcher VERSION: " << STRINGIFY(RegexMatcher_VERSION_MAJOR) << "."
	          << STRINGIFY(RegexMatcher_VERSION_MINOR) << "." << STRINGIFY(RegexMatcher_VERSION_PATCH) << "."
	          << STRINGIFY(RegexMatcher_VERSION_TWEAK) << std::endl;

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
