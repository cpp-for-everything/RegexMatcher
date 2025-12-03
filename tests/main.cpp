#include "utils/test.hpp"

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

int main(int argc, char** argv) {
	std::cout << "RegexMatcher VERSION: " << RegexMatcher_VERSION_MAJOR << "." << RegexMatcher_VERSION_MINOR << "."
	          << RegexMatcher_VERSION_PATCH << "." << RegexMatcher_VERSION_TWEAK << std::endl;

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
