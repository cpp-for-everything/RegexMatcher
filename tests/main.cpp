#include "utils/test.hpp"

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

// Malformed patterns must be rejected with std::invalid_argument, never read past the end.
namespace {
	void expect_invalid_pattern(const std::string& pattern) {
		matcher::RegexMatcher<int, char> root;
		EXPECT_THROW(root.add_regex(pattern, 0), std::invalid_argument) << "pattern: " << pattern;
	}
}  // namespace

TEST(RegexMatcherMalformed, unterminated_limit) {
	expect_invalid_pattern("a{");
	expect_invalid_pattern("a{1");
	expect_invalid_pattern("a{1,2");
}

TEST(RegexMatcherMalformed, unterminated_set) {
	expect_invalid_pattern("[");
	expect_invalid_pattern("[abc");
	expect_invalid_pattern("[a-");
	expect_invalid_pattern("[a\\");
}

TEST(RegexMatcherMalformed, trailing_backslash) {
	expect_invalid_pattern("a\\");
	expect_invalid_pattern("\\");
	expect_invalid_pattern("(a\\");
}

TEST(RegexMatcherMalformed, unclosed_group) {
	expect_invalid_pattern("(");
	expect_invalid_pattern("(a");
	expect_invalid_pattern("a(b(c)");
	expect_invalid_pattern("a(b|c");
}

// Review HIGH findings. Expected behaviour is taken from std::regex where it has the syntax.
namespace {
	std::vector<int> ids_for(const std::string& pattern, const std::string& text) {
		matcher::RegexMatcher<int, char> root;
		root.add_regex(pattern, 0);
		return root.match(text);
	}

	void expect_same_as_std_regex(const std::string& pattern, const std::vector<std::string>& texts) {
		for (const auto& text : texts) {
			const bool expected = std::regex_match(text, std::regex(pattern));
			const bool actual = !ids_for(pattern, text).empty();
			EXPECT_EQ(actual, expected) << "pattern: " << pattern << " text: " << text;
		}
	}
}  // namespace

TEST(RegexMatcherMalformed, unmatched_close_paren) {
	expect_invalid_pattern(")");
	expect_invalid_pattern("a)");
	expect_invalid_pattern("(a))");
	expect_invalid_pattern("a|b)c");
}

TEST(RegexMatcherMalformed, empty_pattern_and_alternatives) {
	expect_invalid_pattern("");
	expect_invalid_pattern("|a");
	expect_invalid_pattern("a|");
	expect_invalid_pattern("a||b");
	expect_invalid_pattern("(|a)");
	expect_invalid_pattern("(a|)");
	expect_invalid_pattern("()");
	expect_invalid_pattern("a()");
}

TEST(RegexMatcherMalformed, nothing_to_repeat) {
	expect_invalid_pattern("*");
	expect_invalid_pattern("+a");
	expect_invalid_pattern("?a");
	expect_invalid_pattern("{2}a");
	expect_invalid_pattern("a|*b");
	expect_invalid_pattern("(*a)");
}

TEST(RegexMatcherGroupRoots, group_starting_with_optional_atom) {
	expect_same_as_std_regex("x(a?b)+", {"xb", "xab", "xbb", "xbxb", "xabab", "xabb", "xbab", "x", "xa", "b"});
	expect_same_as_std_regex("(a?b)+", {"b", "ab", "bb", "abab", "bab", "", "a", "abb"});
	expect_same_as_std_regex("(a*b)*", {"", "b", "ab", "aab", "abab", "bb", "aabb", "a", "ba"});
	expect_same_as_std_regex("x(a*b)*", {"x", "xb", "xab", "xbb", "xbxb", "xa", "xabab"});
}

// Exact counts on a single atom. Counts on groups and counts followed by more pattern are tested below.
TEST(RegexMatcherLimits, exact_count_single_atom) {
	expect_same_as_std_regex("a{3}", {"", "a", "aa", "aaa", "aaaa"});
	expect_same_as_std_regex("a{1}", {"", "a", "aa"});
}

TEST(RegexMatcherLimits, at_least_and_range_single_atom) {
	expect_same_as_std_regex("a{2,}", {"", "a", "aa", "aaa", "aaaa"});
	expect_same_as_std_regex("a{1,3}", {"", "a", "aa", "aaa", "aaaa"});
	expect_same_as_std_regex("a{2,3}", {"", "a", "aa", "aaa", "aaaa"});
	expect_same_as_std_regex("xa{1,2}y", {"xy", "xay", "xaay", "xaaay"});
}

TEST(RegexMatcherMove, moved_matcher_still_matches) {
	matcher::RegexMatcher<int, char> source;
	source.add_regex(std::string("a*"), 0);
	source.add_regex(std::string("(ab)*c"), 1);
	source.add_regex(std::string("b?c"), 2);
	matcher::RegexMatcher<int, char> moved(std::move(source));
	EXPECT_EQ(moved.match(std::string("")), (std::vector<int>{0}));
	EXPECT_EQ(moved.match(std::string("aaa")), (std::vector<int>{0}));
	EXPECT_EQ(moved.match(std::string("ababc")), (std::vector<int>{1}));
	EXPECT_EQ(moved.match(std::string("c")), (std::vector<int>{1, 2}));
	EXPECT_EQ(moved.match(std::string("bc")), (std::vector<int>{2}));
}

TEST(RegexMatcherState, exception_does_not_corrupt_limits) {
	matcher::RegexMatcher<int, char> m;
	m.add_regex(std::string("a{1,3}b"), 0);
	for (int i = 0; i < 3; i++) {
		EXPECT_EQ(m.match(std::string("aab")), (std::vector<int>{0}));
		EXPECT_TRUE(m.match(std::string("aaaab")).empty());
	}
}

TEST(RegexMatcherUnsupported, anchors_and_negated_sets_and_class_escapes) {
	expect_invalid_pattern("^a");
	expect_invalid_pattern("a$");
	expect_invalid_pattern("[^a]");
	expect_invalid_pattern("a\\d");
	expect_invalid_pattern("\\w+");
	expect_invalid_pattern("[a\\s]");
	expect_invalid_pattern("a\\n");
}

TEST(RegexMatcherUnsupported, punctuation_escapes_still_work) {
	expect_same_as_std_regex("a\\.b", {"a.b", "axb"});
	expect_same_as_std_regex("\\/x\\-y", {"/x-y", "x-y"});
	expect_same_as_std_regex("[a\\-c]+", {"a", "-", "b", "a-c"});
}

TEST(RegexMatcherReadme, example_from_readme) {
	matcher::RegexMatcher<int, char> m;
	m.add_regex(std::string("a(bc)*d"), 0);
	m.add_regex(std::string("123[0-9]+abc"), 1);
	EXPECT_EQ(m.match(std::string("123456abc")), (std::vector<int>{1}));
	EXPECT_EQ(m.match(std::string("abcbcd")), (std::vector<int>{0}));
}

// Repeat counters: a minimum is checked when the match leaves the loop, counters live in each match call,
// and matching uses an explicit stack instead of recursion.
TEST(RegexMatcherRepeatMin, minimum_followed_by_more_pattern) {
	EXPECT_TRUE(ids_for("xa{2}y", "xay").empty());
	EXPECT_EQ(ids_for("xa{2}y", "xaay"), (std::vector<int>{0}));
	expect_same_as_std_regex("xa{2}y", {"xy", "xay", "xaay", "xaaay"});
}

TEST(RegexMatcherRepeatMin, range_followed_by_more_pattern) {
	EXPECT_TRUE(ids_for("a{2,3}b", "ab").empty());
	EXPECT_TRUE(ids_for("a{2,3}b", "aaaab").empty());
	EXPECT_EQ(ids_for("a{2,3}b", "aab"), (std::vector<int>{0}));
	EXPECT_EQ(ids_for("a{2,3}b", "aaab"), (std::vector<int>{0}));
	expect_same_as_std_regex("a{2,3}b", {"b", "ab", "aab", "aaab", "aaaab"});
}

TEST(RegexMatcherRepeatMin, group_with_exact_count) {
	EXPECT_TRUE(ids_for("(ab){2}", "ab").empty());
	EXPECT_EQ(ids_for("(ab){2}", "abab"), (std::vector<int>{0}));
	expect_same_as_std_regex("(ab){2}", {"", "ab", "abab", "ababab", "aba"});
}

TEST(RegexMatcherRepeatMin, zero_counts) {
	expect_same_as_std_regex("a{0}", {"", "a", "aa"});
	expect_same_as_std_regex("a{0,1}", {"", "a", "aa"});
	expect_same_as_std_regex("xa{0}y", {"xy", "xay"});
	expect_same_as_std_regex("xa{0,1}y", {"xy", "xay", "xaay"});
	// The skipped atom must not leave edges into nodes that the rest of the pattern uses
	expect_same_as_std_regex("a?a{0}", {"", "a", "aa", "aaa"});
	expect_same_as_std_regex("b*[ab]{0}a*", {"", "a", "b", "ba", "ab", "bba"});
	expect_same_as_std_regex("(ab){0}c", {"c", "abc", ""});
}

TEST(RegexMatcherRepeatMin, alternative_sharing_a_node_with_a_minimum) {
	// "aab" is left out: both alternatives share one node, and the old code accepted it too
	expect_same_as_std_regex("a{2}|ab", {"a", "aa", "ab", "b"});
	expect_same_as_std_regex("ab|a{2}", {"a", "aa", "ab", "b"});
}

TEST(RegexMatcherRepeatMin, nested_groups_with_limits) {
	expect_same_as_std_regex("((ab){1,2}c){2}",
	                         {"abc", "abcabc", "ababcabc", "ababcababc", "abcabcabc", "abababcabc"});
	expect_same_as_std_regex("(a{2}b)+", {"ab", "aab", "aabaab", "aabab", "aaab"});
	expect_same_as_std_regex("(a(bc)*d)+", {"a", "ad", "abcd", "adabcbcd"});
}

TEST(RegexMatcherRepeatMin, backtracking_cases) {
	expect_same_as_std_regex("(a|ab)(c|bcd)", {"abcd", "ac", "abc", "abd"});
	// "a" against a*a is left out: the builder merges the last a into the loop node (a separate defect).
	expect_same_as_std_regex("a*a", {"", "aa", "aaa", "b"});
	expect_same_as_std_regex("x(ab)+y", {"xy", "xaby", "xababy", "xabay"});
}

TEST(RegexMatcherRepeatMin, shared_prefix_with_different_limits) {
	test_for({"ab{2}c", "ab{3}c", "ab+c", "ab{2,3}c", "abc", "ab{2,}c"},
	         {"ac", "abc", "abbc", "abbbc", "abbbbc", "abbbbbc", "abb"});
}

TEST(RegexMatcherState, concurrent_match_on_shared_matcher) {
	const std::vector<std::string> patterns = {"xa{2,3}y", "(ab){2}", "a{1,3}b", "(a{2}b)+", "c*d"};
	const std::vector<std::string> texts = {"xay",   "xaay", "xaaay", "xaaaay", "ab",  "abab",  "ababab",
	                                        "b",     "ab",   "aab",   "aaab",   "aaaab", "aabaab", "cccd", "d"};
	matcher::RegexMatcher<int, char> m;
	for (size_t i = 0; i < patterns.size(); i++) {
		m.add_regex(patterns[i], static_cast<int>(i));
	}
	std::vector<std::vector<int>> expected;
	for (const auto& text : texts) {
		std::vector<int> ids;
		for (size_t i = 0; i < patterns.size(); i++) {
			if (std::regex_match(text, std::regex(patterns[i]))) {
				ids.push_back(static_cast<int>(i));
			}
		}
		expected.push_back(ids);
	}
	constexpr int rounds = 2000;
	std::vector<int> wrong(2, 0);
	auto worker = [&](int index) {
		for (int round = 0; round < rounds; round++) {
			for (size_t t = 0; t < texts.size(); t++) {
				if (m.match(texts[t]) != expected[t]) {
					wrong[index]++;
				}
				if (m.match_with_groups(texts[t]).size() != expected[t].size()) {
					wrong[index]++;
				}
			}
		}
	};
	std::thread first(worker, 0);
	std::thread second(worker, 1);
	first.join();
	second.join();
	EXPECT_EQ(wrong[0], 0);
	EXPECT_EQ(wrong[1], 0);
}

TEST(RegexMatcherLongInput, star_on_long_input) {
	const std::string as(100000, 'a');
	EXPECT_EQ(ids_for("a*", as), (std::vector<int>{0}));
	EXPECT_TRUE(ids_for("a*", as + "b").empty());
	EXPECT_EQ(ids_for("a*b", as + "b"), (std::vector<int>{0}));
}

TEST(RegexMatcherLongInput, group_star_on_long_input) {
	std::string abs;
	for (int i = 0; i < 50000; i++) {
		abs += "ab";
	}
	EXPECT_EQ(ids_for("(ab)*", abs), (std::vector<int>{0}));
	EXPECT_TRUE(ids_for("(ab)*", abs + "a").empty());
	matcher::RegexMatcher<int, char> m;
	m.add_regex(std::string("(ab)*"), 0);
	const auto results = m.match_with_groups(abs);
	ASSERT_EQ(results.size(), 1u);
	EXPECT_EQ(results[0].regex_id, 0);
}

// Wrong answers that the old builder gave because it merged nodes of one regex. Now each regex has
// states of its own after the items it shares with other regexes.
TEST(RegexMatcherMerge, loop_followed_by_the_same_atom) {
	expect_same_as_std_regex("a*a", {"", "a", "aa", "aaa", "b", "ab"});
	expect_same_as_std_regex("a+a", {"", "a", "aa", "aaa"});
	expect_same_as_std_regex("a?a{2}a", {"aa", "aaa", "aaaa", "aaaaa"});
	EXPECT_EQ(ids_for("a*a", "a"), (std::vector<int>{0}));
}

TEST(RegexMatcherMerge, repeat_inside_a_loop) {
	expect_same_as_std_regex("(a{1,2})*", {"", "a", "aa", "aaa", "aaaa", "aaaaa", "b"});
	expect_same_as_std_regex("(a{2})*b", {"b", "ab", "aab", "aaab", "aaaab"});
	EXPECT_EQ(ids_for("(a{1,2})*", "aaa"), (std::vector<int>{0}));
}

TEST(RegexMatcherMerge, alternatives_that_enter_one_node) {
	expect_same_as_std_regex("a{2}|ab", {"a", "aa", "ab", "aab", "b"});
	expect_same_as_std_regex("([ab]a){2,3}|[ab]+[ab]+", {"a", "aa", "ab", "aba", "abab", "bababa", "aaaaaaa"});
	EXPECT_EQ(ids_for("([ab]a){2,3}|[ab]+[ab]+", "aa"), (std::vector<int>{0}));
}

TEST(RegexMatcherMerge, shared_prefix_ending_in_a_loop) {
	test_for({"a*a", "a*b", "a*", "a*a", "(a*)a", "a*ab|a*b"}, {"", "a", "aa", "b", "ab", "aab", "ba"});
}

TEST(RegexMatcherMerge, routes_share_a_prefix) {
	test_for({"/users/[0-9]+", "/users/[0-9]+/posts", "/static/.*", "/api/v1/items/[a-z0-9\\-]+", "/", "/users/me"},
	         {"/", "/users/12", "/users/12/posts", "/users/me", "/users/", "/static/", "/static/a/b.css",
	          "/api/v1/items/x-1", "/api/v1/items/", "/users/12/posts/3"});
}

// ECMAScript: '.' does not match a line terminator
TEST(RegexMatcherSyntax, dot_and_line_terminators) {
	expect_same_as_std_regex("a.b", {"axb", "a\nb", "a\rb", "a.b"});
	expect_same_as_std_regex(".*", {"", "abc", "a\nb"});
}

TEST(RegexMatcherSyntax, sets_and_escapes) {
	const std::vector<std::string> texts = {"a", "-", "b", "c", "d", "e", ".", "]", "a*b", "ab", "a]", "(a)", "x"};
	for (const char* pattern : {"[a-]", "[-a]", "[a-c-e]", "[.]", "[\\]]", "a\\*b", "[\\-a]", "[a\\]]+", "\\(a\\)"}) {
		expect_same_as_std_regex(pattern, texts);
	}
}

TEST(RegexMatcherSyntax, lazy_repeats_and_zero_counts) {
	expect_same_as_std_regex("a*?b", {"b", "ab", "aab", "a"});
	expect_same_as_std_regex("(ab)+?c", {"c", "abc", "ababc"});
	expect_same_as_std_regex("a??a", {"", "a", "aa"});
	expect_same_as_std_regex("a{0,0}b", {"b", "ab"});
	expect_same_as_std_regex("x(a|b){0}y", {"xy", "xay"});
}

TEST(RegexMatcherSyntax, malformed_repeats_are_rejected) {
	expect_invalid_pattern("a**");
	expect_invalid_pattern("a+*");
	expect_invalid_pattern("a{2}{3}");
	expect_invalid_pattern("a*??");
	expect_invalid_pattern("a{2,1}");
	expect_invalid_pattern("a{x}");
	expect_invalid_pattern("a{,2}");
	expect_invalid_pattern("[b-a]");
	expect_invalid_pattern("[]");
	expect_invalid_pattern("(a{1000}){1000}");
}

TEST(RegexMatcherSyntax, rejected_pattern_leaves_matcher_unchanged) {
	matcher::RegexMatcher<int, char> m;
	m.add_regex(std::string("ab*"), 0);
	EXPECT_THROW(m.add_regex(std::string("ab*(c{1000}){1000}"), 1), std::invalid_argument);
	EXPECT_THROW(m.add_regex(std::string("ab*c**"), 2), std::invalid_argument);
	EXPECT_EQ(m.match(std::string("abb")), (std::vector<int>{0}));
	EXPECT_TRUE(m.match(std::string("abbc")).empty());
}

// Captures follow ECMAScript. Where libstdc++ and the Microsoft library disagree with each other, the
// expected values below follow the ECMAScript rules: a new pass of a repeat forgets the captures of the
// last pass, and an optional pass that consumes nothing does not count.
namespace {
	std::map<size_t, std::pair<size_t, size_t>> groups_for(const std::string& pattern, const std::string& text) {
		matcher::RegexMatcher<int, char> m;
		m.add_regex(pattern, 0);
		const auto results = m.match_with_groups(text);
		EXPECT_EQ(results.size(), 1u) << pattern << " on " << text;
		return results.empty() ? std::map<size_t, std::pair<size_t, size_t>>{} : results[0].groups;
	}
	using Groups = std::map<size_t, std::pair<size_t, size_t>>;
}  // namespace

TEST(RegexMatcherGroups, ecmascript_capture_rules) {
	EXPECT_EQ(groups_for("(a?)?", ""), Groups{});
	EXPECT_EQ(groups_for("(a?)*", ""), Groups{});
	EXPECT_EQ(groups_for("(a?){2,4}aa", "aaaa"), (Groups{{0, {1, 2}}}));
	EXPECT_EQ(groups_for("((.*bb*)|[a]+.{1,3})*", "baa"), (Groups{{0, {1, 3}}}));
	EXPECT_EQ(groups_for("(a|ab)(c|bcd)(d*)", "abcd"), (Groups{{0, {0, 1}}, {1, {1, 4}}, {2, {4, 4}}}));
	EXPECT_EQ(groups_for("(a+)(a*)", "aaa"), (Groups{{0, {0, 3}}, {1, {3, 3}}}));
	EXPECT_EQ(groups_for("(a+?)(a*)", "aaa"), (Groups{{0, {0, 1}}, {1, {1, 3}}}));
	EXPECT_EQ(groups_for("(ab)+d", "ababd"), (Groups{{0, {2, 4}}}));
	// A new pass of the loop that starts with an empty (a?|b) comes before the first pass taking b
	EXPECT_EQ(groups_for("((a?|b)(a?|b))*a", "aba"), (Groups{{0, {1, 2}}, {1, {1, 1}}, {2, {1, 2}}}));
}
