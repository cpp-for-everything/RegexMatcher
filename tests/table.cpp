// The lookup table and the entry points that use it: for_each_match, match_into, matches_any,
// compile, set_table_limits and table_stats.

#include <gtest/gtest.h>
#include <matcher/core.hpp>

#include "utils/lookup_paths.hpp"

#include <iostream>
#include <list>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using lookup_paths::Matcher;

	Matcher with_patterns(const std::vector<std::string>& patterns, const matcher::TableLimits& limits = {}) {
		Matcher m;
		m.set_table_limits(limits);
		for (size_t i = 0; i < patterns.size(); i++) {
			m.add_regex(patterns[i], static_cast<int>(i));
		}
		return m;
	}

	matcher::TableLimits off() {
		matcher::TableLimits limits;
		limits.max_states = 0;
		return limits;
	}
}  // namespace

TEST(RegexMatcherTable, entry_points_return_the_ids_of_match) {
	const Matcher m = with_patterns({"a+", "a*", "b", "(a|b)+", "a+"});
	for (const std::string text : {"", "a", "aaa", "b", "ab", "c"}) {
		const std::vector<int> ids = m.match(text);
		std::vector<int> seen;
		EXPECT_EQ(m.for_each_match(text, [&seen](int id) { seen.push_back(id); }), ids.size());
		EXPECT_EQ(seen, ids) << text;
		std::vector<int> into = {42};
		EXPECT_EQ(m.match_into(text, into), ids.size());
		EXPECT_EQ(into, ids) << text;
		EXPECT_EQ(m.matches_any(text), !ids.empty()) << text;
	}
	EXPECT_EQ(m.match(std::string("aaa")), (std::vector<int>{0, 1, 3, 4}));
}

TEST(RegexMatcherTable, callback_returning_false_stops) {
	const Matcher m = with_patterns({"a", "a+", "a*", "(a)"});
	std::vector<int> seen;
	EXPECT_EQ(m.for_each_match(std::string("a"),
	                           [&seen](int id) {
		                           seen.push_back(id);
		                           return seen.size() < 2;
	                           }),
	          2u);
	EXPECT_EQ(seen, (std::vector<int>{0, 1}));
}

TEST(RegexMatcherTable, other_text_types) {
	const Matcher m = with_patterns({"ab+c", "[a-c]+"});
	const std::string_view view = "abbc";
	const std::vector<char> chars = {'a', 'b', 'c'};
	const std::list<char> linked = {'a', 'b', 'b', 'c'};
	EXPECT_EQ(m.match(view), (std::vector<int>{0, 1}));
	EXPECT_EQ(m.match(chars), (std::vector<int>{0, 1}));
	EXPECT_EQ(m.match(linked), (std::vector<int>{0, 1}));
	EXPECT_TRUE(m.matches_any(std::string_view("cab")));
}

// Bytes above 127 are negative in a signed char; the table must read them as the simulation does
TEST(RegexMatcherTable, every_byte_value_agrees_with_the_simulation) {
	const std::vector<std::string> patterns = {".", "[\x80-\xff]", "[\x01-\x7f]+", "a.b", ".+", "\xe9+", "[\xc0-\xdf][\x80-\xbf]"};
	const Matcher table = with_patterns(patterns);
	const Matcher simulation = with_patterns(patterns, off());
	EXPECT_EQ(table.table_stats().mode, matcher::TableMode::full);
	EXPECT_EQ(simulation.table_stats().mode, matcher::TableMode::simulation);
	for (int b = 0; b < 256; b++) {
		for (int c : {-1, 0x41, 0x80, 0xa9, 0xff, int('\n')}) {
			std::string text(1, static_cast<char>(b));
			if (c >= 0) {
				text += static_cast<char>(c);
			}
			EXPECT_EQ(table.match(text), simulation.match(text)) << "bytes " << b << " " << c;
		}
	}
	EXPECT_EQ(table.match(std::string("\xc3\xa9")), (std::vector<int>{4, 6}));
}

// A set whose table explodes: the full build stops, the lazy table answers, with the same ids
TEST(RegexMatcherTable, exploding_table_falls_to_lazy_then_simulation) {
	const std::vector<std::string> patterns = {"(a|b)*a(a|b){12}", "(a|b)*b(a|b){12}", "[ab]*"};
	const Matcher by_default = with_patterns(patterns);
	const matcher::TableStats default_stats = by_default.table_stats();
	std::cout << "[table] default limits: mode " << static_cast<int>(default_stats.mode) << ", "
	          << default_stats.states << " states, " << default_stats.table_bytes << " table bytes, stopped by "
	          << (default_stats.full_stopped_by ? default_stats.full_stopped_by : "nothing") << std::endl;
	matcher::TableLimits small;
	small.max_states = 300;
	const Matcher capped = with_patterns(patterns, small);
	const Matcher simulation = with_patterns(patterns, off());
	EXPECT_EQ(capped.table_stats().mode, matcher::TableMode::lazy);
	EXPECT_STREQ(capped.table_stats().full_stopped_by, "states");
	std::vector<std::string> texts;
	for (unsigned bits = 0; bits < 600; bits++) {
		std::string text;
		for (unsigned k = 0; k < 14 + bits % 5; k++) {
			text += ((bits * 2654435761u) >> (k % 32)) & 1 ? 'a' : 'b';
		}
		texts.push_back(text);
	}
	for (const auto& text : texts) {
		EXPECT_EQ(capped.match(text), simulation.match(text)) << text;
	}
	const matcher::TableStats stats = capped.table_stats();
	EXPECT_LE(stats.states, 300u);
	EXPECT_GT(stats.simulated_texts, 0u);  // the lazy table filled up and some texts were simulated
	for (const auto& text : texts) {
		EXPECT_EQ(capped.match(text), simulation.match(text)) << text;
	}
}

TEST(RegexMatcherTable, compile_builds_once_and_add_regex_rebuilds) {
	Matcher m = with_patterns({"/users/[0-9]+", "/users/me"});
	m.compile();
	matcher::TableStats stats = m.table_stats();
	EXPECT_EQ(stats.mode, matcher::TableMode::full);
	EXPECT_EQ(stats.builds, 1u);
	EXPECT_GT(stats.states, 2u);
	EXPECT_EQ(stats.full_stopped_by, nullptr);
	m.compile();
	EXPECT_EQ(m.match(std::string("/users/me")), (std::vector<int>{1}));
	EXPECT_EQ(m.table_stats().builds, 1u);
	m.add_regex(std::string("/users/[a-z]+"), 2);
	EXPECT_EQ(m.match(std::string("/users/me")), (std::vector<int>{1, 2}));
	EXPECT_EQ(m.table_stats().builds, 2u);
}

TEST(RegexMatcherTable, limits_choose_the_mode) {
	const std::vector<std::string> patterns = {"a(b|c)*d", "ab*"};
	matcher::TableLimits lazy;
	lazy.build_ahead = false;
	Matcher m = with_patterns(patterns, lazy);
	EXPECT_EQ(m.table_stats().mode, matcher::TableMode::lazy);
	EXPECT_STREQ(m.table_stats().full_stopped_by, "off");
	EXPECT_EQ(m.match(std::string("abcbd")), (std::vector<int>{0}));
	EXPECT_EQ(m.match(std::string("abbb")), (std::vector<int>{1}));
	m.set_table_limits(off());
	EXPECT_EQ(m.table_limits().max_states, 0u);
	EXPECT_EQ(m.table_stats().mode, matcher::TableMode::simulation);
	EXPECT_EQ(m.match(std::string("abcbd")), (std::vector<int>{0}));
	m.set_table_limits(matcher::TableLimits{});
	EXPECT_EQ(m.table_stats().mode, matcher::TableMode::full);
	EXPECT_EQ(m.match(std::string("abcbd")), (std::vector<int>{0}));
}

TEST(RegexMatcherTable, empty_matcher_and_empty_text) {
	const Matcher empty;
	EXPECT_TRUE(empty.match(std::string("abc")).empty());
	EXPECT_TRUE(empty.match(std::string()).empty());
	EXPECT_FALSE(empty.matches_any(std::string()));
	const Matcher m = with_patterns({"a*", "b"});
	EXPECT_EQ(m.match(std::string()), (std::vector<int>{0}));
	EXPECT_TRUE(m.matches_any(std::string()));
}

// A capture lookup gives the same answer when the table says no regex matches
TEST(RegexMatcherTable, match_with_groups_same_in_every_mode) {
	const std::vector<std::string> patterns = {"(a+)(b*)", "x(y|z)", "(ab){2}"};
	const Matcher table = with_patterns(patterns);
	const Matcher simulation = with_patterns(patterns, off());
	for (const std::string text : {"", "aab", "xz", "abab", "q", "aaq", "xy"}) {
		EXPECT_EQ(lookup_paths::groups_of(table, text), lookup_paths::groups_of(simulation, text)) << text;
	}
}

// With a char_t wider than 8 bits there is no table; the answers come from the simulation
TEST(RegexMatcherTable, wide_characters_simulate) {
	matcher::RegexMatcher<int, wchar_t> m;
	m.add_regex(std::wstring(L"a.c"), 0);
	m.add_regex(std::wstring(L"[\x100-\x200]+"), 1);
	m.compile();
	EXPECT_EQ(m.table_stats().mode, matcher::TableMode::simulation);
	EXPECT_EQ(m.match(std::wstring(L"a\x150" L"c")), (std::vector<int>{0}));
	EXPECT_EQ(m.match(std::wstring(L"\x150\x1ff")), (std::vector<int>{1}));
	EXPECT_TRUE(m.match(std::wstring(L"a\nc")).empty());
	std::vector<int> into;
	EXPECT_EQ(m.match_into(std::wstring(L"abc"), into), 1u);
	EXPECT_TRUE(m.matches_any(std::wstring(L"\x100")));
}

// Ids that own memory are copied out; nothing is kept after the call
TEST(RegexMatcherTable, string_ids) {
	matcher::RegexMatcher<std::string, char> m;
	m.add_regex(std::string("a+"), "many a");
	m.add_regex(std::string("a"), "one a");
	m.add_regex(std::string("a*"), "many a");
	EXPECT_EQ(m.match(std::string("a")), (std::vector<std::string>{"many a", "one a"}));
	std::vector<std::string> into;
	m.match_into(std::string("aa"), into);
	EXPECT_EQ(into, (std::vector<std::string>{"many a"}));
	m.set_table_limits(off());
	EXPECT_EQ(m.match(std::string("a")), (std::vector<std::string>{"many a", "one a"}));
}
