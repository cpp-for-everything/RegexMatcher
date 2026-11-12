// Many texts against one built matcher, in sequence and from several threads.
//
// An earlier engine changed counters stored in the shared automaton while match() walked it, so two
// texts in two threads gave wrong answers. These tests catch that kind of state on every lookup path:
// each runs for every table configuration (full table, lazy table, lazy table that mostly falls to the
// simulation, simulation only; see utils/lookup_paths.hpp) and every entry point (match,
// for_each_match, match_into, matches_any, match_with_groups with its captures).
//
// The expected answer of each text comes from a fresh matcher that has seen no other text. The tests
// use only the standard library and atomics, so they run under ThreadSanitizer unchanged. They compile
// against the original engine too, with match() and match_with_groups() only.
//
// A heavier run: set REGEXMATCHER_DIFF_SWEEP (the variable of the differential sweep) to a number.

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS  // std::getenv, read only
#endif

#include <gtest/gtest.h>

#include "utils/lookup_paths.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <random>
#include <regex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
	using lookup_paths::Config;
	using lookup_paths::Expected;
	using lookup_paths::Matcher;
	using lookup_paths::Path;

	// Repeats, alternation, groups and shared prefixes, among them the kinds that broke before
	const std::vector<std::string> patterns = {"a{2,3}b",
	                                           "(ab){2}",
	                                           "x(a?b)+",
	                                           "a*a",
	                                           "(a{1,2})*",
	                                           "/users/[0-9]+",
	                                           "/users/[0-9]+/posts",
	                                           "/users/[0-9]+/posts/[0-9]+",
	                                           "/users/me",
	                                           "/users/(me|you)/settings",
	                                           "(a|b)*a(a|b){3}",
	                                           "[a-c]+x?",
	                                           "x(a|ab)*",
	                                           "a{2,}|b{2,}",
	                                           "(a{2}b)+",
	                                           "c*d"};

	std::vector<std::string> make_texts() {
		std::vector<std::string> texts = {"",
		                                  "a",
		                                  "aa",
		                                  "aab",
		                                  "aaab",
		                                  "aaaab",
		                                  "abab",
		                                  "ababab",
		                                  "xb",
		                                  "xab",
		                                  "xabab",
		                                  "xbb",
		                                  "xa",
		                                  "aaaa",
		                                  "abba",
		                                  "babaa",
		                                  "bbbbbb",
		                                  "abcx",
		                                  "aabaab",
		                                  "cccd",
		                                  "d",
		                                  "/users/42",
		                                  "/users/42/posts",
		                                  "/users/42/posts/7",
		                                  "/users/me",
		                                  "/users/me/settings",
		                                  "/users/you/settings",
		                                  "/users/",
		                                  "/users/x",
		                                  "zzzz",
		                                  "q",
		                                  "/v"};
		texts.push_back(std::string(300, 'a'));
		texts.push_back(std::string(300, 'a') + "b");
		texts.push_back("/users/" + std::string(200, '7'));
		texts.push_back("/users/" + std::string(200, '7') + "/posts/1");
		return texts;
	}

	size_t stress() {
		const char* value = std::getenv("REGEXMATCHER_DIFF_SWEEP");
		return (value == nullptr || *value == '\0' || std::strtoull(value, nullptr, 10) == 0) ? 1 : 20;
	}

	Matcher make_matcher(const Config& config, const std::vector<std::string>& pats) {
		Matcher m;
		lookup_paths::apply(m, config);
		lookup_paths::add_all(m, pats);
		return m;
	}

	// Lets all threads start their work at the same moment (C++17 has no std::latch)
	class StartGate {
	public:
		explicit StartGate(size_t threads) : remaining_(threads) {}
		void arrive_and_wait() {
			std::unique_lock<std::mutex> lock(mutex_);
			if (--remaining_ == 0) {
				cv_.notify_all();
				return;
			}
			cv_.wait(lock, [this] { return remaining_ == 0; });
		}

	private:
		std::mutex mutex_;
		std::condition_variable cv_;
		size_t remaining_;
	};

	// Counts wrong answers from any thread and keeps the first one
	struct Mismatches {
		std::atomic<size_t> count{0};
		std::mutex mutex;
		std::string first;

		void add(const std::string& what) {
			if (count.fetch_add(1) == 0) {
				std::lock_guard<std::mutex> lock(mutex);
				first = what;
			}
		}
		void check(const Matcher& m, Path path, const std::string& text, const Expected& expected,
		           const std::string& where) {
			if (!lookup_paths::check(m, path, text, expected)) {
				add(where + " path " + lookup_paths::name(path) + " text \"" + text.substr(0, 40) + "\"");
			}
		}
	};

	// Runs body(thread_index) on `threads` threads that start together
	void run_threads(size_t threads, const std::function<void(size_t)>& body) {
		StartGate gate(threads);
		std::vector<std::thread> pool;
		for (size_t t = 0; t < threads; t++) {
			pool.emplace_back([&gate, &body, t] {
				gate.arrive_and_wait();
				body(t);
			});
		}
		for (auto& thread : pool) {
			thread.join();
		}
	}

	constexpr size_t thread_count = 8;

	// The texts of thread t: its own share of the list, different from every other thread's
	std::vector<size_t> texts_of_thread(size_t t, size_t text_count) {
		std::vector<size_t> own;
		for (size_t i = t; i < text_count; i += thread_count) {
			own.push_back(i);
		}
		return own;
	}

#ifdef REGEXMATCHER_TEST_TABLE_API
	size_t builds_of(const Matcher& m) { return m.table_stats().builds; }
#endif
}  // namespace

// The expected values themselves agree with std::regex_match. Short texts only: std::regex backtracks,
// and (a{1,2})* on the long texts takes it exponential time.
TEST(RegexMatcherSequence, expected_values_agree_with_std_regex) {
	const auto texts = make_texts();
	constexpr size_t longest = 40;
	for (const auto& config : lookup_paths::configs()) {
		const auto expected = lookup_paths::expected_on_fresh(patterns, config, texts);
		for (size_t t = 0; t < texts.size(); t++) {
			if (texts[t].size() > longest) {
				continue;
			}
			std::vector<int> want;
			for (size_t i = 0; i < patterns.size(); i++) {
				if (std::regex_match(texts[t], std::regex(patterns[i]))) {
					want.push_back(static_cast<int>(i));
				}
			}
			EXPECT_EQ(expected[t].ids, want) << config.name << " text " << texts[t];
		}
	}
}

// 1. One matcher, many texts in many orders: every call gives what a fresh matcher gives
TEST(RegexMatcherSequence, no_state_carried_between_calls) {
	const auto texts = make_texts();
	const size_t n = texts.size();
	std::vector<std::vector<size_t>> orders;
	std::vector<size_t> forward(n);
	for (size_t i = 0; i < n; i++) {
		forward[i] = i;
	}
	orders.push_back(forward);
	orders.emplace_back(forward.rbegin(), forward.rend());
	std::vector<size_t> shuffled = forward;
	std::shuffle(shuffled.begin(), shuffled.end(), std::mt19937(2024));
	orders.push_back(shuffled);
	std::vector<size_t> repeated;
	for (size_t i = 0; i < n; i++) {
		repeated.insert(repeated.end(), 5, i);
	}
	orders.push_back(repeated);
	for (const auto& config : lookup_paths::configs()) {
		const auto expected = lookup_paths::expected_on_fresh(patterns, config, texts);
		// A failing text between two matching texts, and a long text between two short ones
		std::vector<size_t> between;
		const size_t failing = static_cast<size_t>(
		    std::find_if(expected.begin(), expected.end(), [](const Expected& e) { return e.ids.empty(); }) -
		    expected.begin());
		ASSERT_LT(failing, n);
		for (size_t i = 0; i < n; i++) {
			if (!expected[i].ids.empty()) {
				between.insert(between.end(), {i, failing, i});
			}
			if (texts[i].size() > 100) {
				between.insert(between.end(), {1, i, 2});
			}
		}
		auto all_orders = orders;
		all_orders.push_back(between);

		const Matcher m = make_matcher(config, patterns);
		Mismatches wrong;
		for (size_t round = 0; round < stress(); round++) {
			for (const auto& order : all_orders) {
				for (const size_t i : order) {
					for (const auto path : lookup_paths::paths()) {
						wrong.check(m, path, texts[i], expected[i], config.name);
					}
				}
			}
		}
#ifdef REGEXMATCHER_TEST_TABLE_API
		// A callback that stops at the first id leaves nothing behind
		for (size_t i = 0; i < n; i++) {
			if (expected[i].ids.size() < 2) {
				continue;
			}
			size_t seen = 0;
			const size_t calls = m.for_each_match(texts[i], [&seen](int) {
				seen++;
				return false;
			});
			if (calls != 1 || seen != 1) {
				wrong.add(config.name + " stopping callback");
			}
			for (const size_t j : {i, failing, i}) {
				for (const auto path : lookup_paths::paths()) {
					wrong.check(m, path, texts[j], expected[j], config.name + " after a stopped callback");
				}
			}
		}
#endif
		EXPECT_EQ(wrong.count.load(), 0u) << wrong.first;
	}
}

#ifdef REGEXMATCHER_TEST_TABLE_API
// A callback may look up again on the same thread, on the same or another matcher
TEST(RegexMatcherSequence, callback_that_looks_up_again) {
	const auto texts = make_texts();
	for (const auto& config : lookup_paths::configs()) {
		const auto expected = lookup_paths::expected_on_fresh(patterns, config, texts);
		const Matcher m = make_matcher(config, patterns);
		Mismatches wrong;
		for (size_t i = 0; i < texts.size(); i++) {
			std::vector<int> ids;
			m.for_each_match(texts[i], [&](int id) {
				ids.push_back(id);
				const size_t j = (i + 7) % texts.size();
				wrong.check(m, Path::callback, texts[j], expected[j], config.name + " inner");
			});
			if (ids != expected[i].ids) {
				wrong.add(config.name + " outer " + texts[i]);
			}
		}
		EXPECT_EQ(wrong.count.load(), 0u) << wrong.first;
	}
}
#endif

namespace {
	// 2. Threads on one shared matcher, each on its own texts; `path_of(t)` picks each thread's path
	void different_texts_per_thread(const std::function<Path(size_t)>& path_of, size_t rounds) {
		const auto texts = make_texts();
		for (const auto& config : lookup_paths::configs()) {
			const auto expected = lookup_paths::expected_on_fresh(patterns, config, texts);
			const Matcher m = make_matcher(config, patterns);
			Mismatches wrong;
			run_threads(thread_count, [&](size_t t) {
				const auto own = texts_of_thread(t, texts.size());
				const Path path = path_of(t);
				for (size_t round = 0; round < rounds * stress(); round++) {
					for (const size_t i : own) {
						wrong.check(m, path, texts[i], expected[i], config.name + " thread " + std::to_string(t));
					}
				}
			});
			EXPECT_EQ(wrong.count.load(), 0u) << config.name << ": " << wrong.first;
		}
	}
}  // namespace

TEST(RegexMatcherThreads, different_texts_match) {
	different_texts_per_thread([](size_t) { return Path::match; }, 150);
}

TEST(RegexMatcherThreads, different_texts_match_with_groups) {
	different_texts_per_thread([](size_t) { return Path::groups; }, 40);
}

TEST(RegexMatcherThreads, different_texts_every_entry_point_at_once) {
	const auto paths = lookup_paths::paths();
	different_texts_per_thread([&paths](size_t t) { return paths[t % paths.size()]; }, 100);
}

// 3. The first lookup of a fresh matcher from several threads at the same moment builds the table once
TEST(RegexMatcherThreads, first_use_under_contention) {
	const auto texts = make_texts();
	const auto paths = lookup_paths::paths();
	for (const auto& config : lookup_paths::configs()) {
		const auto expected = lookup_paths::expected_on_fresh(patterns, config, texts);
		Mismatches wrong;
		for (size_t round = 0; round < 40 * stress(); round++) {
			const Matcher m = make_matcher(config, patterns);
			run_threads(thread_count, [&](size_t t) {
				const size_t i = (t * 5 + round) % texts.size();  // a different text in each thread
				const Path path = paths[(t + round) % paths.size()];
				wrong.check(m, path, texts[i], expected[i], config.name + " first lookup");
				const size_t j = (i + 1) % texts.size();
				wrong.check(m, path, texts[j], expected[j], config.name + " second lookup");
			});
#ifdef REGEXMATCHER_TEST_TABLE_API
			if (builds_of(m) != 1) {
				wrong.add(config.name + " table built " + std::to_string(builds_of(m)) + " times");
			}
#endif
		}
		EXPECT_EQ(wrong.count.load(), 0u) << wrong.first;
	}
}

// 4. Several matchers of different sizes used from several threads, so no buffer of a thread is shared
// between matchers by mistake
TEST(RegexMatcherThreads, many_matchers_many_threads) {
	std::vector<std::vector<std::string>> sets = {patterns, {"a", "b", "ab"}, {"(a|b)*a(a|b){6}", "(a|b)*b(a|b){6}", "x*"}};
	std::vector<std::string> routes;
	for (int i = 0; i < 40; i++) {
		routes.push_back("/r" + std::to_string(i) + "/[0-9]+(/[a-z]+)?");
	}
	sets.push_back(routes);
	std::vector<std::string> texts = make_texts();
	for (int i = 0; i < 40; i += 3) {
		texts.push_back("/r" + std::to_string(i) + "/12");
		texts.push_back("/r" + std::to_string(i) + "/12/abc");
	}
	texts.push_back("abababababbabaabab");
	texts.push_back("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbba");
	for (const auto& config : lookup_paths::configs()) {
		std::vector<Matcher> matchers;
		std::vector<std::vector<Expected>> expected;
		for (const auto& set : sets) {
			matchers.push_back(make_matcher(config, set));
			expected.push_back(lookup_paths::expected_on_fresh(set, config, texts));
		}
		const auto paths = lookup_paths::paths();
		Mismatches wrong;
		run_threads(thread_count, [&](size_t t) {
			const auto own = texts_of_thread(t, texts.size());
			for (size_t round = 0; round < 30 * stress(); round++) {
				for (size_t k = 0; k < matchers.size(); k++) {
					const size_t s = (k + t) % matchers.size();  // each thread in its own order
					for (const size_t i : own) {
						const Path path = paths[(i + round) % paths.size()];
						wrong.check(matchers[s], path, texts[i], expected[s][i],
						            config.name + " matcher " + std::to_string(s));
					}
				}
			}
		});
		EXPECT_EQ(wrong.count.load(), 0u) << wrong.first;
	}
}

// 5. Copies and moves give the same answers; a copy made and used while the original is in use stays right.
// The original engine is move only, so against it only the moves are checked.
TEST(RegexMatcherThreads, copy_and_move) {
	const auto texts = make_texts();
	const auto paths = lookup_paths::paths();
	for (const auto& config : lookup_paths::configs()) {
		const auto expected = lookup_paths::expected_on_fresh(patterns, config, texts);
		Matcher original = make_matcher(config, patterns);
		Mismatches wrong;
		const auto check_all = [&](const Matcher& m, const std::string& what) {
			for (size_t i = 0; i < texts.size(); i++) {
				for (const auto path : paths) {
					wrong.check(m, path, texts[i], expected[i], config.name + " " + what);
				}
			}
		};
		check_all(original, "original");

#ifndef REGEXMATCHER_TEST_TABLE_API
		Matcher moved(std::move(original));
		check_all(moved, "moved");
		Matcher move_assigned;
		move_assigned = std::move(moved);
		check_all(move_assigned, "move assigned");
		run_threads(4, [&](size_t t) {
			for (size_t round = 0; round < 20 * stress(); round++) {
				for (size_t i = t; i < texts.size(); i += 4) {
					wrong.check(move_assigned, paths[(i + round) % paths.size()], texts[i], expected[i],
					            config.name + " moved, thread " + std::to_string(t));
				}
			}
		});
#else
		std::atomic<bool> stop{false};
		std::vector<Matcher> copies;
		copies.reserve(4);
		std::thread user([&] {  // keeps using the original while the copies are made and used
			size_t i = 0;
			while (!stop.load()) {
				wrong.check(original, paths[i % paths.size()], texts[i % texts.size()], expected[i % texts.size()],
				            config.name + " original in use");
				i++;
			}
		});
		for (int c = 0; c < 4; c++) {
			copies.push_back(original);  // copy made while the original is in use
		}
		run_threads(4, [&](size_t t) {
			for (size_t round = 0; round < 20 * stress(); round++) {
				for (size_t i = t; i < texts.size(); i += 4) {
					wrong.check(copies[t], paths[(i + round) % paths.size()], texts[i], expected[i],
					            config.name + " copy " + std::to_string(t));
				}
			}
		});
		stop.store(true);
		user.join();

		Matcher assigned;
		assigned = copies[0];
		check_all(assigned, "copy assigned");
		const Matcher moved(std::move(copies[1]));
		check_all(moved, "moved");
		Matcher move_assigned;
		move_assigned = std::move(copies[2]);
		check_all(move_assigned, "move assigned");
		check_all(original, "original after the copies");
		// Each copy builds its own table; a move takes the table along
		if (builds_of(original) != 1 || builds_of(copies[3]) != 1 || builds_of(assigned) != 1 ||
		    builds_of(moved) != 1 || builds_of(move_assigned) != 1) {
			wrong.add(config.name + " table builds after copy and move");
		}
#endif
		EXPECT_EQ(wrong.count.load(), 0u) << wrong.first;
	}
}

// 6. add_regex after lookups: the new regex is found, old answers stay right, no stale table is used
TEST(RegexMatcherThreads, lookups_after_add_regex) {
	const auto texts = make_texts();
	const auto paths = lookup_paths::paths();
	const std::vector<std::string> first(patterns.begin(), patterns.end() - 3);
	for (const auto& config : lookup_paths::configs()) {
		const auto expected_before = lookup_paths::expected_on_fresh(first, config, texts);
		const auto expected_after = lookup_paths::expected_on_fresh(patterns, config, texts);
		Matcher m = make_matcher(config, first);
		Mismatches wrong;
		const auto threads = [&](const std::vector<Expected>& expected, const std::string& what) {
			run_threads(thread_count, [&](size_t t) {
				for (size_t round = 0; round < 10 * stress(); round++) {
					for (const size_t i : texts_of_thread(t, texts.size())) {
						wrong.check(m, paths[(i + t) % paths.size()], texts[i], expected[i], config.name + " " + what);
					}
				}
			});
		};
		threads(expected_before, "before add_regex");
		for (size_t i = first.size(); i < patterns.size(); i++) {
			m.add_regex(patterns[i], static_cast<int>(i));  // single threaded, as documented
		}
		threads(expected_after, "after add_regex");
#ifdef REGEXMATCHER_TEST_TABLE_API
		if (builds_of(m) != 2) {
			wrong.add(config.name + " table built " + std::to_string(builds_of(m)) + " times, expected 2");
		}
#endif
		EXPECT_EQ(wrong.count.load(), 0u) << wrong.first;
	}
}
