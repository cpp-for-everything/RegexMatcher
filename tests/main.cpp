#include "utils/test.hpp"

TEST(RegexMatcherValidity, matcher_urls)
{
	test_for({"/", "\\/([0-9a-z\\-]+)"}, {"/", "/12asdf-"});
}

TEST(RegexMatcherValidity, matcher_test_no_regex)
{
	test_for({"text1", "text2", "text3"}, {"text", "text1", "text2", "text3", "text4"});
}

TEST(RegexMatcherValidity, matcher_test_group)
{
	test_for({"text(1)", "text(2)", "text(3)"}, {"text", "text1", "text2", "text3", "text4"});
}

TEST(RegexMatcherValidity, matcher_test_global_or)
{
	test_for({"text1|test2|test3"}, {"text", "text1", "text2", "text3", "text4"});
}

TEST(RegexMatcherValidity, matcher_test_or_within_group)
{
	test_for({"text(1|2|3)"}, {"text", "text1", "text2", "text3", "text4"});
}

TEST(RegexMatcherValidity, matcher_test_letter_asterix)
{
	test_for({"text1*"}, {"text", "text1", "text2", "text3", "text4"});
}

TEST(RegexMatcherValidity, matcher_test_group_asterix)
{
	test_for({"text(1)*"}, {"text", "text1", "text2", "text3", "text4"});
}

TEST(RegexMatcherValidity, matcher_test_group_with_or_asterix)
{
	test_for({"text(1|2|3)*"}, {"text", "text1", "text2", "text3", "text4"});
}

TEST(RegexMatcherValidity, matcher_test_group_with_or_plus)
{
	test_for({"text(1|2|3)+"}, {"text", "text1", "text21", "text31", "text34"});
}

TEST(RegexMatcherValidity, matcher_test_group_with_repeat)
{
	test_for({"text(1|2|3){1,2}"}, {"text", "text1", "text21", "text31", "text34", "text123123"});
}

TEST(RegexMatcherValidity, matcher_test_group_with_question_mark)
{
	test_for({"text(1|2|3)?"}, {"text", "text1", "text21", "text31", "text34", "text123123"});
}

TEST(RegexMatcherValidity, matcher_tests_many_regexes_many_matches)
{
	test_for(
		{"d(abc|def)*g+", "d(abc)*g+", "a?", "b|c", "(d|e)f", "f[a-c]?d(ab|cd)*g+", "a{1,3}a", "aaa", "aa"},
		{"a",         "aa",        "aaa",       "aaaa",    "a",        "b",          "c",          "d",          "df",       "e",
		 "ef",        "fdg",       "fdgg",      "fd",      "fdabgg",   "fdababgg",   "fdabcdgg",   "fdcdcdgg",   "fdacgg",   "fadabgg",
		 "fadababgg", "fadabcdgg", "fadcdcdgg", "fadacgg", "fbdabgg",  "fbdababgg",  "fbdabcdgg",  "fbdcdcdgg",  "fbdacgg",  "fcdabgg",
		 "fcdababgg", "fcdabcdgg", "fcdcdcdgg", "fcdacgg", "fccdabgg", "fccdababgg", "fccdabcdgg", "fccdcdcdgg", "fccdacgg", "dab",
		 "dabcabc",   "dabc",      "ddefdef",   "dabcg",   "dg",       "dabcabcg",   "ddefabcg",   "ddefdefg"});
}

int main(int argc, char** argv)
{
	std::cout << "RegexMatcher VERSION: " << RegexMatcher_VERSION_MAJOR << "." << RegexMatcher_VERSION_MINOR << "." << RegexMatcher_VERSION_PATCH << "."
			  << RegexMatcher_VERSION_TWEAK << std::endl;

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
