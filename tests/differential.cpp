// Differential tests: the matcher against std::regex_match (ECMAScript) on random patterns.
//
// The generator writes patterns over the supported syntax: the literals a and b, '.', sets, groups,
// alternation and the repeats *, +, ?, {n}, {n,} and {n,m} with small numbers, nested a few levels.
// Each pattern is checked on every string over {a, b} up to a short length, or over {a, b, c} up to a
// shorter one, so that '.' differs from [ab].
//
// Modes:
//   single  each pattern alone in a fresh matcher
//   multi   several unrelated patterns in one matcher
//   prefix  several patterns that start with the same items in one matcher, so they share states
//
// Every mode builds one matcher per table configuration of utils/lookup_paths.hpp (full table, lazy
// table, lazy table of 4 states that mostly falls to the simulation, simulation only) and checks each
// against std::regex. It also checks that for_each_match(), match_into() and matches_any() agree with
// match(), that match_with_groups() returns the same ids as match() and the same captures in every
// configuration, and counts the matches whose captures differ from std::smatch (reported, see the
// README for the known difference).
//
// The normal run uses fixed seeds and stays short. A larger sweep runs when the environment sets
// REGEXMATCHER_DIFF_SWEEP to a number of seeds; REGEXMATCHER_DIFF_PATTERNS (patterns per seed) and
// REGEXMATCHER_DIFF_LENGTH (longest text) change its size. Most of a sweep's time goes to the
// reference: libstdc++ std::regex backtracks, and a few random patterns take it seconds per text at
// length 6, so the defaults stay at 500 patterns and length 5.

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS  // std::getenv, read only
#endif

#include <gtest/gtest.h>
#include <matcher/core.hpp>

#include "utils/lookup_paths.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <random>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
	// Patterns from a seed. Only the raw output of std::mt19937 is used, which the standard fixes,
	// so the same seed gives the same patterns with every standard library.
	class PatternGenerator {
	public:
		explicit PatternGenerator(uint32_t seed) : rng_(seed) {}

		std::string pattern() { return alternation(max_depth); }

		// Items without a top level '|', to build patterns that share a prefix
		std::string sequence() { return concatenation(max_depth); }

	private:
		static constexpr int max_depth = 2;

		size_t pick(size_t n) { return static_cast<size_t>(rng_() % n); }

		std::string alternation(int depth) {
			std::string out = concatenation(depth);
			if (pick(4) == 0) {
				out += "|" + concatenation(depth);
			}
			return out;
		}

		std::string concatenation(int depth) {
			std::string out;
			const size_t items = 1 + pick(3);
			for (size_t i = 0; i < items; i++) {
				out += atom(depth) + repeat();
			}
			return out;
		}

		std::string atom(int depth) {
			static const char* const sets[] = {"[ab]", "[a]", "[b]", "[a-b]", "[ba]"};
			switch (pick(depth > 0 ? 7 : 5)) {
				case 0:
				case 1:
					return "a";
				case 2:
					return "b";
				case 3:
					return ".";
				case 4:
					return sets[pick(5)];
				default:
					return "(" + alternation(depth - 1) + ")";
			}
		}

		std::string repeat() {
			switch (pick(10)) {
				case 4:
					return "*";
				case 5:
					return "+";
				case 6:
					return "?";
				case 7:
					return "{" + std::to_string(pick(3)) + "}";
				case 8:
					return "{" + std::to_string(pick(3)) + ",}";
				case 9: {
					const size_t n = pick(3);
					return "{" + std::to_string(n) + "," + std::to_string(n + pick(3)) + "}";
				}
				default:
					return "";
			}
		}

		std::mt19937 rng_;
	};

	// Every string over the alphabet with up to max_length characters, the empty string first.
	std::vector<std::string> all_texts(size_t max_length, const std::string& alphabet = "ab") {
		std::vector<std::string> texts = {""};
		for (size_t begin = 0, length = 1; length <= max_length; length++) {
			const size_t end = texts.size();
			for (size_t i = begin; i < end; i++) {
				for (const char c : alphabet) {
					texts.push_back(texts[i] + c);
				}
			}
			begin = end;
		}
		return texts;
	}

	size_t env_number(const char* name, size_t fallback) {
		const char* value = std::getenv(name);
		if (value == nullptr || *value == '\0') {
			return fallback;
		}
		return static_cast<size_t>(std::strtoull(value, nullptr, 10));
	}

	// Matchers of one configuration by the mode their table ended in
	struct ModeCount {
		size_t full = 0;
		size_t lazy = 0;
		size_t simulation = 0;
		size_t simulated_texts = 0;  // lazy lookups that fell to the simulation
	};

	struct Tally {
		size_t checked = 0;           // (pattern, text, configuration) answers compared
		size_t wrong = 0;             // answers of match() that differ from std::regex
		size_t groups_differ = 0;     // texts where match_with_groups() and match() give different ids
		size_t paths_differ = 0;      // lookups of the other entry points that differ from match()
		size_t groups_mode_differ = 0;  // texts where captures differ between configurations
		size_t captures_checked = 0;  // matching (pattern, text) pairs whose captures were compared
		size_t captures_wrong = 0;    // of those, the ones whose captures differ from std::smatch
		size_t rejected = 0;          // patterns that add_regex refused
		std::string first;            // the first wrong answer
		std::string first_capture;    // the first capture difference
		std::vector<ModeCount> modes;  // by configuration

		void add(const Tally& other) {
			checked += other.checked;
			wrong += other.wrong;
			groups_differ += other.groups_differ;
			captures_checked += other.captures_checked;
			captures_wrong += other.captures_wrong;
			rejected += other.rejected;
			paths_differ += other.paths_differ;
			groups_mode_differ += other.groups_mode_differ;
			modes.resize(std::max(modes.size(), other.modes.size()));
			for (size_t c = 0; c < other.modes.size(); c++) {
				modes[c].full += other.modes[c].full;
				modes[c].lazy += other.modes[c].lazy;
				modes[c].simulation += other.modes[c].simulation;
				modes[c].simulated_texts += other.modes[c].simulated_texts;
			}
			if (first.empty()) {
				first = other.first;
			}
			if (first_capture.empty()) {
				first_capture = other.first_capture;
			}
		}
	};

	std::string describe_groups(const std::map<size_t, std::pair<size_t, size_t>>& groups) {
		std::string out;
		for (const auto& [id, span] : groups) {
			out += " " + std::to_string(id) + ":(" + std::to_string(span.first) + "," + std::to_string(span.second) +
			       ")";
		}
		return out.empty() ? " none" : out;
	}

	// Captures of std::smatch in the matcher's form: group ids from 0, unmatched groups left out.
	std::map<size_t, std::pair<size_t, size_t>> std_groups(const std::smatch& m) {
		std::map<size_t, std::pair<size_t, size_t>> groups;
		for (size_t g = 1; g < m.size(); g++) {
			if (m[g].matched) {
				const auto start = static_cast<size_t>(m.position(g));
				groups[g - 1] = {start, start + static_cast<size_t>(m.length(g))};
			}
		}
		return groups;
	}

	// Checks the matchers (one per configuration) that hold `patterns` (id i is patterns[i]; an empty
	// string marks a pattern that was rejected) on every text.
	Tally check_matchers(const std::vector<lookup_paths::Matcher>& matchers,
	                     const std::vector<lookup_paths::Config>& configs, const std::vector<std::string>& patterns,
	                     const std::vector<std::string>& texts) {
		Tally tally;
		std::vector<std::regex> references;
		for (const auto& pattern : patterns) {
			references.emplace_back(pattern.empty() ? std::string("x") : pattern);
		}
		std::vector<std::smatch> reference_matches(patterns.size());
		std::vector<bool> expected(patterns.size());
		for (const auto& text : texts) {
			for (size_t i = 0; i < patterns.size(); i++) {
				expected[i] = !patterns[i].empty() && std::regex_match(text, reference_matches[i], references[i]);
			}
			std::vector<std::pair<int, lookup_paths::Groups>> first_groups;
			for (size_t c = 0; c < matchers.size(); c++) {
				const auto& m = matchers[c];
				const std::vector<int> got = m.match(text);
				const auto with_groups = lookup_paths::groups_of(m, text);
				std::vector<int> group_ids;
				for (const auto& result : with_groups) {
					group_ids.push_back(result.first);
				}
				if (group_ids != got) {
					tally.groups_differ++;
				}
				if (c == 0) {
					first_groups = with_groups;
				} else if (with_groups != first_groups) {
					tally.groups_mode_differ++;
				}
				const lookup_paths::Expected from_match{got, with_groups};
				for (const auto path : lookup_paths::paths()) {
					if (path != lookup_paths::Path::match && !lookup_paths::check(m, path, text, from_match)) {
						tally.paths_differ++;
					}
				}
				for (size_t i = 0; i < patterns.size(); i++) {
					if (patterns[i].empty()) {
						continue;
					}
					const int id = static_cast<int>(i);
					const bool matched = std::find(got.begin(), got.end(), id) != got.end();
					tally.checked++;
					if (expected[i] != matched) {
						tally.wrong++;
						if (tally.first.empty()) {
							tally.first = "config " + configs[c].name + " pattern \"" + patterns[i] + "\" text \"" + text +
							              "\" expected " + (expected[i] ? "match" : "no match") + " got " +
							              (matched ? "match" : "no match");
						}
						continue;
					}
					if (c != 0 || !expected[i]) {
						continue;  // captures against std::smatch: the first configuration only
					}
					const auto result = std::find_if(with_groups.begin(), with_groups.end(),
					                                 [id](const auto& r) { return r.first == id; });
					if (result == with_groups.end()) {
						continue;
					}
					tally.captures_checked++;
					const auto expected_groups = std_groups(reference_matches[i]);
					if (expected_groups != result->second) {
						tally.captures_wrong++;
						if (tally.first_capture.empty()) {
							tally.first_capture = "pattern \"" + patterns[i] + "\" text \"" + text + "\" std::regex" +
							                      describe_groups(expected_groups) + " got" +
							                      describe_groups(result->second);
						}
					}
				}
			}
		}
		tally.modes.resize(matchers.size());
#ifdef REGEXMATCHER_TEST_TABLE_API
		for (size_t c = 0; c < matchers.size(); c++) {
			const matcher::TableStats stats = matchers[c].table_stats();
			auto& count = tally.modes[c];
			count.full += stats.mode == matcher::TableMode::full ? 1 : 0;
			count.lazy += stats.mode == matcher::TableMode::lazy ? 1 : 0;
			count.simulation += stats.mode == matcher::TableMode::simulation ? 1 : 0;
			count.simulated_texts += stats.simulated_texts;
		}
#endif
		return tally;
	}

	// Adds the patterns to one matcher per configuration with ids 0, 1, ... and checks them.
	Tally check_patterns(std::vector<std::string> patterns, const std::vector<std::string>& texts) {
		const auto configs = lookup_paths::configs();
		std::vector<lookup_paths::Matcher> matchers(configs.size());
		for (size_t c = 0; c < configs.size(); c++) {
			lookup_paths::apply(matchers[c], configs[c]);
		}
		size_t rejected = 0;
		for (size_t i = 0; i < patterns.size(); i++) {
			try {
				for (auto& m : matchers) {
					m.add_regex(patterns[i], static_cast<int>(i));
				}
			} catch (const std::invalid_argument&) {
				patterns[i].clear();
				rejected++;
			}
		}
		Tally tally = check_matchers(matchers, configs, patterns, texts);
		tally.rejected += rejected;
		return tally;
	}

	// Each pattern alone in a fresh matcher.
	Tally single_mode(uint32_t seed, size_t patterns, const std::vector<std::string>& texts) {
		PatternGenerator generator(seed);
		Tally tally;
		for (size_t p = 0; p < patterns; p++) {
			tally.add(check_patterns({generator.pattern()}, texts));
		}
		return tally;
	}

	// Several unrelated patterns in one matcher.
	Tally multi_mode(uint32_t seed, size_t patterns, size_t per_matcher, const std::vector<std::string>& texts) {
		PatternGenerator generator(seed);
		Tally tally;
		for (size_t done = 0; done < patterns; done += per_matcher) {
			std::vector<std::string> group;
			for (size_t i = 0; i < per_matcher && done + i < patterns; i++) {
				group.push_back(generator.pattern());
			}
			tally.add(check_patterns(group, texts));
		}
		return tally;
	}

	constexpr size_t prefix_set_size = 9;

	// Patterns that start with the same items, so the matcher shares their states: the prefix alone, the
	// prefix with different endings, the same pattern under two ids, the prefix as a group, a pattern with
	// alternatives that both start with the prefix, and the prefix followed by a or a*.
	Tally prefix_mode(uint32_t seed, size_t sets, const std::vector<std::string>& texts) {
		PatternGenerator generator(seed);
		Tally tally;
		for (size_t s = 0; s < sets; s++) {
			const std::string prefix = generator.sequence();
			const std::string first = generator.pattern();
			const std::string second = generator.sequence();
			const std::string third = generator.sequence();
			const std::vector<std::string> patterns = {prefix + "(" + first + ")",
			                                           prefix,
			                                           prefix + second,
			                                           prefix + "(" + first + ")",
			                                           prefix + second + "|" + prefix + third,
			                                           prefix + "a",
			                                           prefix + "a*",
			                                           "(" + prefix + ")" + third,
			                                           prefix + third};
			tally.add(check_patterns(patterns, texts));
		}
		return tally;
	}

	void report(const char* mode, uint32_t seed, size_t patterns, const Tally& tally) {
		std::cout << "[differential] " << mode << " seed " << seed << " patterns " << patterns << ": "
		          << tally.wrong << " wrong of " << tally.checked << " answers, " << tally.groups_differ
		          << " texts where match_with_groups ids differ, " << tally.captures_wrong
		          << " capture differences of " << tally.captures_checked << " compared, " << tally.rejected
		          << " patterns rejected, " << tally.paths_differ << " entry point differences, "
		          << tally.groups_mode_differ << " capture differences between configurations" << std::endl;
		const auto configs = lookup_paths::configs();
		for (size_t c = 0; c < tally.modes.size() && c < configs.size(); c++) {
			std::cout << "[differential]   config " << configs[c].name << ": matchers full " << tally.modes[c].full
			          << ", lazy " << tally.modes[c].lazy << ", simulation " << tally.modes[c].simulation
			          << "; lazy lookups that simulated " << tally.modes[c].simulated_texts << std::endl;
		}
		if (!tally.first.empty()) {
			std::cout << "[differential] first wrong: " << tally.first << std::endl;
		}
		if (!tally.first_capture.empty()) {
			std::cout << "[differential] first capture difference: " << tally.first_capture << std::endl;
		}
	}

	void expect_clean(const Tally& tally, const std::string& what) {
		EXPECT_EQ(tally.wrong, 0u) << what << ": " << tally.first;
		EXPECT_EQ(tally.groups_differ, 0u) << what;
		EXPECT_EQ(tally.rejected, 0u) << what;
		EXPECT_EQ(tally.paths_differ, 0u) << what;
		EXPECT_EQ(tally.groups_mode_differ, 0u) << what;
#ifdef REGEXMATCHER_TEST_TABLE_API
		// Every lookup path ran: full tables, lazy tables, lazy lookups that fell to the simulation, and
		// matchers that only simulate
		ASSERT_EQ(tally.modes.size(), 4u) << what;
		EXPECT_GT(tally.modes[0].full, 0u) << what;
		EXPECT_EQ(tally.modes[1].full + tally.modes[1].simulation, 0u) << what;
		EXPECT_GT(tally.modes[1].lazy, 0u) << what;
		EXPECT_GT(tally.modes[2].simulated_texts, 0u) << what;
		EXPECT_EQ(tally.modes[3].full + tally.modes[3].lazy, 0u) << what;
		EXPECT_GT(tally.modes[3].simulation, 0u) << what;
#endif
	}

	constexpr size_t default_length = 5;
	constexpr size_t abc_length = 4;
	constexpr size_t multi_per_matcher = 8;
}  // namespace

TEST(RegexMatcherDifferential, single_pattern_seed_12345) {
	constexpr uint32_t seed = 12345;
	constexpr size_t patterns = 400;
	const Tally tally = single_mode(seed, patterns, all_texts(default_length));
	report("single", seed, patterns, tally);
	expect_clean(tally, "seed 12345");
}

TEST(RegexMatcherDifferential, single_pattern_other_seeds) {
	for (const uint32_t seed : {1u, 2u, 3u}) {
		constexpr size_t patterns = 200;
		const Tally tally = single_mode(seed, patterns, all_texts(default_length));
		report("single", seed, patterns, tally);
		expect_clean(tally, "seed " + std::to_string(seed));
	}
}

// Texts with a character that no literal of the patterns has, so '.' differs from [ab]
TEST(RegexMatcherDifferential, single_pattern_alphabet_abc) {
	constexpr uint32_t seed = 4;
	constexpr size_t patterns = 200;
	const Tally tally = single_mode(seed, patterns, all_texts(abc_length, "abc"));
	report("single abc", seed, patterns, tally);
	expect_clean(tally, "seed 4");
}

TEST(RegexMatcherDifferential, multi_pattern_shared_matcher) {
	for (const uint32_t seed : {12345u, 1u, 2u, 3u}) {
		constexpr size_t patterns = 400;
		const Tally tally = multi_mode(seed, patterns, multi_per_matcher, all_texts(default_length));
		report("multi", seed, patterns, tally);
		expect_clean(tally, "seed " + std::to_string(seed));
	}
}

TEST(RegexMatcherDifferential, multi_pattern_common_prefix) {
	for (const uint32_t seed : {12345u, 5u}) {
		constexpr size_t sets = 40;
		const Tally tally = prefix_mode(seed, sets, all_texts(default_length));
		report("prefix", seed, sets * prefix_set_size, tally);
		expect_clean(tally, "seed " + std::to_string(seed));
	}
}

// The larger sweep, off unless REGEXMATCHER_DIFF_SWEEP is set.
TEST(RegexMatcherDifferential, sweep_from_environment) {
	const size_t seeds = env_number("REGEXMATCHER_DIFF_SWEEP", 0);
	if (seeds == 0) {
		GTEST_SKIP() << "set REGEXMATCHER_DIFF_SWEEP to a number of seeds to run the sweep";
	}
	const size_t patterns = env_number("REGEXMATCHER_DIFF_PATTERNS", 500);
	const size_t length = env_number("REGEXMATCHER_DIFF_LENGTH", 5);
	const auto texts = all_texts(length);
	const auto abc_texts = all_texts(length > 1 ? length - 1 : length, "abc");
	Tally total;
	for (uint32_t seed = 100; seed < 100 + seeds; seed++) {
		const Tally single = single_mode(seed, patterns, texts);
		report("single", seed, patterns, single);
		const Tally abc = single_mode(seed + 1000, patterns / 4, abc_texts);
		report("single abc", seed + 1000, patterns / 4, abc);
		const Tally multi = multi_mode(seed, patterns, multi_per_matcher, texts);
		report("multi", seed, patterns, multi);
		const Tally prefix = prefix_mode(seed, patterns / prefix_set_size, texts);
		report("prefix", seed, patterns / prefix_set_size * prefix_set_size, prefix);
		total.add(single);
		total.add(abc);
		total.add(multi);
		total.add(prefix);
	}
	report("sweep total", 100, patterns, total);
	expect_clean(total, "sweep");
}
