// The lookup paths of RegexMatcher and the table configurations, so that a test can run the same
// checks on every path. With the original engine (no matcher/impl/table.ipp) only match() and
// match_with_groups() exist and there is one configuration; the tests compile against it unchanged.
#pragma once

#include <matcher/core.hpp>

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#if __has_include(<matcher/impl/table.ipp>)
#define REGEXMATCHER_TEST_TABLE_API 1
#endif

namespace lookup_paths {
	using Matcher = matcher::RegexMatcher<int, char>;
	using Groups = std::map<size_t, std::pair<size_t, size_t>>;

	// How a matcher is set up: which table mode its lookups use
	struct Config {
		std::string name;
#ifdef REGEXMATCHER_TEST_TABLE_API
		matcher::TableLimits limits;
#endif
	};

	// full: the default, a full table when it fits. lazy: a lazy table. lazy_tiny: a full build that
	// stops at once and a lazy table of 4 states, so most texts fall to the simulation in the middle of
	// a walk. simulation: no table.
	inline std::vector<Config> configs() {
#ifdef REGEXMATCHER_TEST_TABLE_API
		matcher::TableLimits lazy;
		lazy.build_ahead = false;
		matcher::TableLimits tiny;
		tiny.max_states = 4;
		matcher::TableLimits off;
		off.max_states = 0;
		return {{"full", matcher::TableLimits{}}, {"lazy", lazy}, {"lazy_tiny", tiny}, {"simulation", off}};
#else
		return {{"engine"}};
#endif
	}

	inline void apply(Matcher& m, const Config& config) {
#ifdef REGEXMATCHER_TEST_TABLE_API
		m.set_table_limits(config.limits);
#else
		(void)m;
		(void)config;
#endif
	}

	enum class Path { match, callback, into, any, groups };

	inline std::vector<Path> paths() {
#ifdef REGEXMATCHER_TEST_TABLE_API
		return {Path::match, Path::callback, Path::into, Path::any, Path::groups};
#else
		return {Path::match, Path::groups};
#endif
	}

	inline const char* name(Path path) {
		switch (path) {
			case Path::match:
				return "match";
			case Path::callback:
				return "for_each_match";
			case Path::into:
				return "match_into";
			case Path::any:
				return "matches_any";
			case Path::groups:
				return "match_with_groups";
		}
		return "?";
	}

	// What a lookup of one text must give
	struct Expected {
		std::vector<int> ids;
		std::vector<std::pair<int, Groups>> groups;

		bool operator==(const Expected& other) const { return ids == other.ids && groups == other.groups; }
	};

	inline std::vector<std::pair<int, Groups>> groups_of(const Matcher& m, const std::string& text) {
		std::vector<std::pair<int, Groups>> out;
		for (const auto& result : m.match_with_groups(text)) {
			out.emplace_back(result.regex_id, result.groups);
		}
		return out;
	}

	// Adds the patterns with ids 0, 1, ...; a pattern the engine rejects is left out.
	inline void add_all(Matcher& m, const std::vector<std::string>& patterns) {
		for (size_t i = 0; i < patterns.size(); i++) {
			try {
				m.add_regex(patterns[i], static_cast<int>(i));
			} catch (const std::exception&) {
			}
		}
	}

	// The answer for one text on a fresh matcher that has seen no other text
	inline Expected expected_on_fresh(const std::vector<std::string>& patterns, const Config& config,
	                                  const std::string& text) {
		Matcher m;
		apply(m, config);
		add_all(m, patterns);
		return {m.match(text), groups_of(m, text)};
	}

	inline std::vector<Expected> expected_on_fresh(const std::vector<std::string>& patterns, const Config& config,
	                                               const std::vector<std::string>& texts) {
		std::vector<Expected> out;
		for (const auto& text : texts) {
			out.push_back(expected_on_fresh(patterns, config, text));
		}
		return out;
	}

	// One lookup of `text` on `path`; true when it gives what `expected` says. For match_with_groups the
	// captures are compared too.
	inline bool check(const Matcher& m, Path path, const std::string& text, const Expected& expected) {
		switch (path) {
			case Path::match:
				return m.match(text) == expected.ids;
			case Path::groups:
				return groups_of(m, text) == expected.groups;
#ifdef REGEXMATCHER_TEST_TABLE_API
			case Path::callback: {
				std::vector<int> ids;
				const size_t calls = m.for_each_match(text, [&ids](int id) { ids.push_back(id); });
				return ids == expected.ids && calls == ids.size();
			}
			case Path::into: {
				std::vector<int> ids = {-7, -8};  // old contents must go
				const size_t count = m.match_into(text, ids);
				return ids == expected.ids && count == ids.size();
			}
			case Path::any:
				return m.matches_any(text) == !expected.ids.empty();
#endif
			default:
				return false;
		}
	}
}  // namespace lookup_paths
