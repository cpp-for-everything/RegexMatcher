// Tests of matcher::DynamicSet (include/matcher/dynamic.hpp).
//
// - Fixture replay: minimal DFAs, operation sequences and reference counts exported from the Python
//   prototype of paper p10 (core/dyn.py). Small fixtures are in tests/data/dynamic; the large ones are
//   read from the folder named by REGEXMATCHER_DYNAMIC_FIXTURES when it is set. Every count is compared
//   after every operation, and the structure is compared with an independent from-scratch product.
// - Random add and remove sequences in every layout, isomorphism after every step.
// - The two open verification items of the theory notes: updates while records of several older
//   class maps are alive, and the in-degree counters after class splits, recomputed and compared.
// - The counterexamples of the two wrong garbage rules.
// - Lookups against std::regex (ECMAScript) and against RegexMatcher, and edge cases.

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS  // std::getenv, read only
#endif

#include <gtest/gtest.h>
#include <matcher/core.hpp>
#include <matcher/dynamic.hpp>

#include "dynamic_util.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <list>
#include <random>
#include <regex>
#include <set>
#include <string>
#include <vector>

#ifndef REGEXMATCHER_TEST_DATA_DIR
#define REGEXMATCHER_TEST_DATA_DIR "tests/data"
#endif

namespace {
	using matcher::DynamicLayout;
	using matcher::DynamicOptions;
	using matcher::DynamicSet;
	using matcher::MinimalDfa;
	using matcher::UpdateStats;

	struct LayoutCase {
		const char* name;
		DynamicOptions options;
	};

	DynamicOptions make_options(DynamicLayout layout, unsigned slots = 16) {
		DynamicOptions o;
		o.layout = layout;
		o.slots = slots;
		o.slot_reserve_log2 = 22;  // small reservations for the tests
		return o;
	}

	std::vector<LayoutCase> all_layouts() {
		return {{"L1_W16", make_options(DynamicLayout::slot_tagged, 16)},
		        {"L1_W3", make_options(DynamicLayout::slot_tagged, 3)},
		        {"L1_W2", make_options(DynamicLayout::slot_tagged, 2)},
		        {"L1_W1", make_options(DynamicLayout::slot_tagged, 1)},
		        {"L2", make_options(DynamicLayout::dense_spare)},
		        {"L3", make_options(DynamicLayout::full_bytes)}};
	}

	template <typename Id>
	std::string compare_with_scratch(const DynamicSet<Id>& set, const std::map<Id, const MinimalDfa*>& current) {
		std::vector<std::pair<Id, const MinimalDfa*>> pats(current.begin(), current.end());
		const auto scratch = dyntest::scratch_product(pats);
		return dyntest::isomorphic(dyntest::from_snapshot(set.snapshot()), scratch);
	}

	// The represented transitions of every record, to check that survivors keep them (NW)
	template <typename Id>
	std::map<uint32_t, std::vector<uint32_t>> rows_of(const DynamicSet<Id>& set) {
		std::map<uint32_t, std::vector<uint32_t>> rows;
		for (const uint32_t r : set.records()) {
			auto& row = rows[r];
			for (size_t b = 0; b < 256; b++) {
				row.push_back(set.transition(r, static_cast<uint8_t>(b)));
			}
		}
		return rows;
	}

	// Exact counts the proofs give for every update
	std::string proof_counts(const UpdateStats& st) {
		std::string bad;
		if (st.old_rows_written != 0) {
			bad += "old rows written; ";
		}
		if (st.is_add) {
			if (st.created != st.live_region) {
				bad += "created != live region; ";
			}
			if (st.garbage_row_reads != 2 * st.candidates) {
				bad += "garbage row reads != 2|C|; ";
			}
			if (st.deleted > st.candidates || st.candidates > st.created) {
				bad += "garbage bound; ";
			}
			if (!st.empty_language && st.old_rows_read != st.live_region) {
				bad += "old rows read != live pairs; ";
			}
		} else if (st.created > st.live_region || st.deleted != st.live_region) {
			bad += "remove counts; ";
		}
		return bad;
	}

	struct ReplayTally {
		size_t fixtures = 0;
		size_t ops = 0;
		size_t count_mismatches = 0;
		size_t class_differences_after_empty_language = 0;
		size_t iso_checks = 0;
		size_t iso_failures = 0;
		size_t invariant_failures = 0;
		size_t splits_checked = 0;
		size_t recycles = 0;
		size_t max_slots = 0;
		size_t dfa_size_mismatches = 0;
		size_t nw_violations = 0;
		std::vector<std::string> first_problems;
		void problem(const std::string& s) {
			if (first_problems.size() < 12) {
				first_problems.push_back(s);
			}
		}
	};

	// Replays one fixture. check_every: isomorphism and invariants every that many operations (1: all),
	// and always at the end.
	void replay(const dyntest::Fixture& fx, const DynamicOptions& options, size_t check_every, bool deep,
	            ReplayTally& tally) {
		tally.fixtures++;
		DynamicSet<uint32_t> set(options);
		std::map<uint32_t, const MinimalDfa*> current;
		std::vector<MinimalDfa> minimal(fx.patterns.size());
		for (size_t i = 0; i < fx.patterns.size(); i++) {
			minimal[i] = matcher::minimize_dfa(fx.patterns[i].dfa);
			const bool same = minimal[i].state_count == fx.patterns[i].dfa.state_count &&
			                  (minimal[i].dead == MinimalDfa::none) == (fx.patterns[i].dead_in_file == MinimalDfa::none);
			if (!same) {
				tally.dfa_size_mismatches++;
				tally.problem(fx.name + ": pattern " + std::to_string(i) + " is not minimal in C++ terms");
			}
		}
		const bool compare_classes = options.layout != DynamicLayout::full_bytes;
		bool empty_language_seen = false;
		for (size_t step = 0; step < fx.ops.size(); step++) {
			const auto& op = fx.ops[step];
			const auto before = check_every == 1 ? rows_of(set) : std::map<uint32_t, std::vector<uint32_t>>();
			UpdateStats st;
			if (op.kind == 'a') {
				st = set.add_dfa(fx.patterns[op.pid].dfa, op.pid);
				current[op.pid] = &minimal[op.pid];
				empty_language_seen = empty_language_seen || st.empty_language;
			} else {
				st = set.remove(op.pid);
				current.erase(op.pid);
			}
			tally.ops++;
			const std::string where = fx.name + " op " + std::to_string(step) + " (" + op.kind + " " +
			                          std::to_string(op.pid) + ")";
			const size_t ref_deleted = op.deleted;
			const bool counts_equal = st.states == op.states && st.live_region == op.live &&
			                          st.created == op.created && st.deleted == ref_deleted &&
			                          (op.kind == 'a' ? st.candidates == op.extra : st.merged == op.extra);
			if (!counts_equal) {
				tally.count_mismatches++;
				tally.problem(where + ": states " + std::to_string(st.states) + "/" + std::to_string(op.states) +
				              " live " + std::to_string(st.live_region) + "/" + std::to_string(op.live) + " created " +
				              std::to_string(st.created) + "/" + std::to_string(op.created) + " deleted " +
				              std::to_string(st.deleted) + "/" + std::to_string(op.deleted));
			}
			if (compare_classes && st.classes != op.classes) {
				// The proofs test the empty language before REFINE; dyn.py refines first (claims-status F7).
				if (empty_language_seen && st.classes < op.classes) {
					tally.class_differences_after_empty_language++;
				} else {
					tally.count_mismatches++;
					tally.problem(where + ": classes " + std::to_string(st.classes) + "/" + std::to_string(op.classes));
				}
			}
			const std::string pc = proof_counts(st);
			if (!pc.empty()) {
				tally.count_mismatches++;
				tally.problem(where + ": " + pc);
			}
			tally.recycles += st.slot_recycles;
			tally.max_slots = std::max(tally.max_slots, st.slots_in_use);
			const bool last = step + 1 == fx.ops.size();
			if (check_every != 0 && (step % check_every == 0 || last)) {
				const std::string inv = set.check_invariants(deep);
				if (!inv.empty()) {
					tally.invariant_failures++;
					tally.problem(where + ": invariant: " + inv.substr(0, 200));
				}
				if (st.class_splits) {
					tally.splits_checked++;
				}
				tally.iso_checks++;
				const std::string iso = compare_with_scratch(set, current);
				if (!iso.empty()) {
					tally.iso_failures++;
					tally.problem(where + ": not isomorphic to the scratch product: " + iso);
				}
				if (check_every == 1) {
					// NW: every surviving record keeps its represented transitions. A record number that
					// exists before and after one update is a survivor: freed numbers are reused only by
					// later updates.
					const auto after = rows_of(set);
					for (const auto& kv : after) {
						const auto it = before.find(kv.first);
						if (it != before.end() && it->second != kv.second) {
							tally.nw_violations++;
							tally.problem(where + ": record " + std::to_string(kv.first) + " changed its transitions");
						}
					}
				}
			}
		}
	}

	std::string data_dir() { return std::string(REGEXMATCHER_TEST_DATA_DIR) + "/dynamic"; }

	std::vector<std::string> fixture_files(const std::string& dir) {
		std::vector<std::string> files;
		for (const auto& e : std::filesystem::directory_iterator(dir)) {
			if (e.path().extension() == ".fix") {
				files.push_back(e.path().string());
			}
		}
		std::sort(files.begin(), files.end());
		return files;
	}

	void report(const char* what, const char* layout, const ReplayTally& t) {
		std::cout << "[dynamic] " << what << " " << layout << ": fixtures " << t.fixtures << ", operations " << t.ops
		          << ", count mismatches " << t.count_mismatches << ", class differences after an empty language "
		          << t.class_differences_after_empty_language << ", isomorphism checks " << t.iso_checks
		          << " (failed " << t.iso_failures << "), invariant failures " << t.invariant_failures
		          << ", split operations checked " << t.splits_checked << ", slot recycles " << t.recycles
		          << ", most slots in use " << t.max_slots << ", non-minimal inputs " << t.dfa_size_mismatches << "\n";
		for (const auto& p : t.first_problems) {
			std::cout << "  " << p << "\n";
		}
	}
}  // namespace

// ---------------------------------------------------------------- fixture replay

TEST(DynamicFixtures, SmallFixturesEveryLayout) {
	const auto files = fixture_files(data_dir());
	ASSERT_FALSE(files.empty()) << "no fixtures in " << data_dir();
	for (const auto& lc : all_layouts()) {
		ReplayTally t;
		for (const auto& f : files) {
			for (const auto& fx : dyntest::read_fixtures(f)) {
				const bool small = fx.patterns.size() <= 12;
				replay(fx, lc.options, 1, small, t);
			}
		}
		report("small fixtures", lc.name, t);
		EXPECT_EQ(t.count_mismatches, 0u) << lc.name;
		EXPECT_EQ(t.iso_failures, 0u) << lc.name;
		EXPECT_EQ(t.invariant_failures, 0u) << lc.name;
		EXPECT_EQ(t.dfa_size_mismatches, 0u) << lc.name;
		EXPECT_EQ(t.nw_violations, 0u) << lc.name;
		EXPECT_GT(t.ops, 1000u);
	}
}

TEST(DynamicFixtures, LargeFixturesFromEnvironment) {
	const char* dir = std::getenv("REGEXMATCHER_DYNAMIC_FIXTURES");
	if (dir == nullptr) {
		GTEST_SKIP() << "set REGEXMATCHER_DYNAMIC_FIXTURES to the folder of the large fixtures";
	}
	const std::vector<LayoutCase> layouts = {{"L1_W16", make_options(DynamicLayout::slot_tagged, 16)},
	                                         {"L2", make_options(DynamicLayout::dense_spare)}};
	for (const auto& lc : layouts) {
		ReplayTally t;
		DynamicOptions o = lc.options;
		o.slot_reserve_log2 = 26;
		for (const auto& f : fixture_files(dir)) {
			for (const auto& fx : dyntest::read_fixtures(f)) {
				const size_t every = std::max<size_t>(1, fx.ops.size() / 24);
				replay(fx, o, every, false, t);
			}
		}
		report("large fixtures", lc.name, t);
		EXPECT_EQ(t.count_mismatches, 0u);
		EXPECT_EQ(t.iso_failures, 0u);
		EXPECT_EQ(t.invariant_failures, 0u);
	}
}

// ---------------------------------------------------------------- random sequences

namespace {
	// Patterns over a, b and c in the library's syntax
	std::string random_pattern(std::mt19937& rng, int depth = 2) {
		const auto pick = [&](size_t n) { return static_cast<size_t>(rng() % n); };
		const char* atoms[] = {"a", "b", "c", ".", "[ab]", "[bc]", "(ab)", "(a|c)"};
		std::string out;
		const size_t items = 1 + pick(3);
		for (size_t i = 0; i < items; i++) {
			if (depth > 0 && pick(4) == 0) {
				out += "(" + random_pattern(rng, depth - 1) + "|" + random_pattern(rng, depth - 1) + ")";
			} else {
				out += atoms[pick(8)];
			}
			const char* reps[] = {"", "", "", "*", "+", "?", "{2}", "{1,2}"};
			out += reps[pick(8)];
		}
		return out;
	}
}  // namespace

TEST(DynamicRandom, AddRemoveIsomorphicEveryStep) {
	size_t steps = 0;
	size_t splits = 0;
	size_t recycles = 0;
	for (const auto& lc : all_layouts()) {
		for (uint32_t seed = 1; seed <= 12; seed++) {
			std::mt19937 rng(seed * 7919u);
			DynamicSet<int> set(lc.options);
			std::map<int, MinimalDfa> dfas;
			std::map<int, const MinimalDfa*> current;
			int next_id = 0;
			for (int step = 0; step < 40; step++) {
				const bool add = current.empty() || rng() % 3 != 0;
				UpdateStats st;
				if (add) {
					const std::string p = random_pattern(rng);
					const int id = next_id++;
					dfas[id] = matcher::compile_regex_dfa(p);
					st = set.add_regex(p, id);
					current[id] = &dfas[id];
				} else {
					auto it = current.begin();
					std::advance(it, rng() % current.size());
					const int id = it->first;
					st = set.remove(id);
					current.erase(id);
				}
				steps++;
				splits += st.class_splits;
				recycles += st.slot_recycles;
				ASSERT_EQ(proof_counts(st), "") << lc.name << " seed " << seed << " step " << step;
				ASSERT_EQ(set.check_invariants(true), "") << lc.name << " seed " << seed << " step " << step;
				ASSERT_EQ(compare_with_scratch(set, current), "") << lc.name << " seed " << seed << " step " << step;
			}
		}
	}
	std::cout << "[dynamic] random sequences: steps " << steps << ", splits " << splits << ", slot recycles "
	          << recycles << "\n";
	EXPECT_GT(recycles, 0u);
}

// ---------------------------------------------------------------- open verification items

// Item 1: ADD and REMOVE read records through older class maps. Every pattern below adds a new byte
// class, so records of many slots are alive at once. With W = 1 to 3 slots are recycled.
TEST(DynamicOpenItems, UpdatesThroughOlderClassMaps) {
	const std::vector<std::string> pats = {"a*", "b(a|b)*", "c[ab]*c", "(ab|d)*", "e+a", "f?(a|e)*",
	                                       "[a-d]+g", "h(ab)*h", "(i|a)*j", "k[^]*" /* unsupported, skipped */,
	                                       "l.*", "m(a|b|c)*m", "(n|o)p", "q(r|s)*", "t[a-t]*u"};
	for (unsigned slots : {16u, 3u, 2u, 1u}) {
		DynamicSet<int> set(make_options(DynamicLayout::slot_tagged, slots));
		std::map<int, MinimalDfa> dfas;
		std::map<int, const MinimalDfa*> current;
		size_t max_slots = 0, recycles = 0, reads_from_old_slots = 0;
		int id = 0;
		const auto check = [&](const UpdateStats& st, const std::string& what) {
			max_slots = std::max(max_slots, st.slots_in_use);
			recycles += st.slot_recycles;
			ASSERT_EQ(proof_counts(st), "") << what;
			ASSERT_EQ(set.check_invariants(true), "") << what;
			ASSERT_EQ(compare_with_scratch(set, current), "") << what;
		};
		for (const auto& p : pats) {
			MinimalDfa d;
			try {
				d = matcher::compile_regex_dfa(p);
			} catch (const std::invalid_argument&) {
				continue;
			}
			dfas[id] = d;
			current[id] = &dfas[id];
			const auto st = set.add_regex(p, id);
			reads_from_old_slots += st.old_rows_read;
			check(st, "add " + p);
			id++;
		}
		// remove every other pattern, then add them back: these read rows of older slots
		for (int i = 0; i < id; i += 2) {
			if (!current.count(i)) {
				continue;
			}
			current.erase(i);
			check(set.remove(i), "remove " + std::to_string(i));
		}
		for (int i = 0; i < id; i += 2) {
			if (!dfas.count(i)) {
				continue;
			}
			current[i + 1000] = &dfas[i];
			check(set.add_dfa(dfas[i], i + 1000), "re-add " + std::to_string(i));
		}
		std::cout << "[dynamic] older class maps, W = " << slots << ": most slots in use " << max_slots
		          << ", slot recycles " << recycles << ", classes " << set.class_count() << "\n";
		if (slots == 16) {
			EXPECT_GE(max_slots, 5u);
		} else {
			EXPECT_GT(recycles, 0u);
		}
	}
}

// Item 2: the incremental counter rule on a class split, against counters recomputed from scratch
// (check_invariants recomputes every counter). Splits in all layouts, many of them with live records.
TEST(DynamicOpenItems, CountersAfterClassSplits) {
	for (const auto& lc : all_layouts()) {
		if (lc.options.layout == DynamicLayout::full_bytes) {
			continue;  // never splits
		}
		size_t splits = 0;
		for (uint32_t seed = 100; seed < 130; seed++) {
			std::mt19937 rng(seed);
			DynamicSet<int> set(lc.options);
			std::map<int, MinimalDfa> dfas;
			for (int i = 0; i < 14; i++) {
				// every pattern uses fresh letters, so most adds split a class
				std::string p;
				const char base = static_cast<char>('a' + (rng() % 20));
				p += base;
				p += "(";
				p += static_cast<char>(base + 1 + rng() % 5);
				p += "|";
				p += static_cast<char>('A' + rng() % 26);
				p += ")*";
				if (rng() % 2) {
					p += static_cast<char>('0' + rng() % 10);
				}
				const auto st = set.add_regex(p, i);
				if (st.class_splits) {
					splits++;
					ASSERT_EQ(set.check_invariants(false), "") << lc.name << " " << p;
				}
				if (i % 4 == 3) {
					set.remove(i - 2);
					ASSERT_EQ(set.check_invariants(false), "") << lc.name;
				}
			}
		}
		std::cout << "[dynamic] counters after splits, " << lc.name << ": splits checked " << splits << "\n";
		EXPECT_GT(splits, 100u);
	}
}

// ---------------------------------------------------------------- the wrong garbage rules

namespace {
	// Sigma* a over all 256 bytes: 2 states, accepting after an 'a'
	MinimalDfa sigma_star_a() {
		MinimalDfa d;
		d.state_count = 2;
		d.class_count = 2;
		for (size_t b = 0; b < 256; b++) {
			d.byte_class[b] = b == 'a' ? 1 : 0;
		}
		d.delta = {0, 1, 0, 1};
		d.accepting = {0, 1};
		d.initial = 0;
		return d;
	}
}  // namespace

TEST(DynamicGarbage, GreatestFixpointCounterexamples) {
	for (const auto& lc : all_layouts()) {
		{
			// P empty, add Sigma*: the old initial record has a self loop and must be deleted
			DynamicSet<int> set(lc.options);
			const auto st = set.add_dfa(dyntest::one_state_dfa(true), 1);
			EXPECT_EQ(st.candidates, 1u);
			EXPECT_EQ(st.deleted, 1u) << lc.name;
			EXPECT_EQ(set.state_count(), 1u);
			EXPECT_EQ(set.check_invariants(true), "");
		}
		{
			// P = {Sigma* a}, add Sigma*: a 2-cycle of old records with no entry, both deleted
			DynamicSet<int> set(lc.options);
			const MinimalDfa a = sigma_star_a();
			const MinimalDfa all = dyntest::one_state_dfa(true);
			set.add_dfa(a, 1);
			ASSERT_EQ(set.state_count(), 2u);
			const auto st = set.add_dfa(all, 2);
			EXPECT_EQ(st.candidates, 2u);
			EXPECT_EQ(st.deleted, 2u) << lc.name;
			EXPECT_EQ(set.state_count(), 2u);
			EXPECT_EQ(set.check_invariants(true), "");
			EXPECT_EQ(compare_with_scratch(set, std::map<int, const MinimalDfa*>{{1, &a}, {2, &all}}), "");
		}
		{
			// P = {(ab)*}, add (ab)* under a new id: the two old records of the cycle are deleted (the
			// dead record stays, reached through exits)
			DynamicSet<int> set(lc.options);
			set.add_regex(std::string("(ab)*"), 1);
			const size_t before = set.state_count();
			ASSERT_EQ(before, 3u);
			const auto st = set.add_regex(std::string("(ab)*"), 2);
			// C holds the two cycle records only: the dead record never pairs with a live new component
			EXPECT_EQ(st.candidates, 2u) << lc.name;
			EXPECT_EQ(st.deleted, 2u) << lc.name;
			EXPECT_EQ(set.state_count(), before);
			EXPECT_EQ(set.check_invariants(true), "");
		}
	}
}

TEST(DynamicGarbage, ExitTargetSeedingCounterexample) {
	// P = {a|ba}, add {a}: s_eps is reached on "ba" through s_b outside C and must stay; s_0 goes.
	for (const auto& lc : all_layouts()) {
		DynamicSet<int> set(lc.options);
		const MinimalDfa d1 = matcher::compile_regex_dfa(std::string("a|ba"));
		const MinimalDfa d2 = matcher::compile_regex_dfa(std::string("a"));
		set.add_dfa(d1, 1);
		ASSERT_EQ(set.state_count(), 4u);
		const auto st = set.add_dfa(d2, 2);
		EXPECT_EQ(st.candidates, 2u) << lc.name;
		EXPECT_EQ(st.deleted, 1u) << lc.name;
		EXPECT_EQ(set.check_invariants(true), "");
		EXPECT_EQ(compare_with_scratch(set, std::map<int, const MinimalDfa*>{{1, &d1}, {2, &d2}}), "");
		EXPECT_EQ(set.match(std::string("ba")), std::vector<int>{1});
		EXPECT_EQ(set.match(std::string("a")), (std::vector<int>{1, 2}));
	}
}

// ---------------------------------------------------------------- lookups

namespace {
	std::vector<std::string> all_strings(const std::string& alphabet, size_t max_len) {
		std::vector<std::string> out{""};
		for (size_t i = 0; i < out.size(); i++) {
			if (out[i].size() < max_len) {
				for (const char c : alphabet) {
					out.push_back(out[i] + c);
				}
			}
		}
		return out;
	}
}  // namespace

TEST(DynamicLookup, AgreesWithStdRegexAndRegexMatcher) {
	const auto texts = all_strings("abc\n", 4);
	size_t compared = 0;
	for (const auto& lc : all_layouts()) {
		for (uint32_t seed = 1; seed <= 25; seed++) {
			std::mt19937 rng(seed * 104729u);
			DynamicSet<int> set(lc.options);
			matcher::RegexMatcher<int, char> rm;
			std::vector<std::regex> refs;
			std::vector<std::string> pats;
			for (int i = 0; i < 6; i++) {
				const std::string p = random_pattern(rng);
				set.add_regex(p, i);
				rm.add_regex(p, i);
				refs.emplace_back(p, std::regex::ECMAScript);
				pats.push_back(p);
			}
			// remove one and add it back, so the lookups also run after a removal
			set.remove(2);
			set.add_regex(pats[2], 2);
			for (const auto& t : texts) {
				std::vector<int> want;
				for (int i = 0; i < 6; i++) {
					if (std::regex_match(t, refs[i])) {
						want.push_back(i);
					}
				}
				ASSERT_EQ(set.match(t), want) << lc.name << " text '" << t << "' seed " << seed;
				ASSERT_EQ(rm.match(t), want);
				std::vector<int> into;
				EXPECT_EQ(set.match_into(t, into), want.size());
				EXPECT_EQ(into, want);
				EXPECT_EQ(set.matches_any(t), !want.empty());
				std::vector<int> called;
				EXPECT_EQ(set.for_each_match(t, [&](int id) { called.push_back(id); }), want.size());
				EXPECT_EQ(called, want);
				compared++;
			}
		}
	}
	std::cout << "[dynamic] lookups compared with std::regex and RegexMatcher: " << compared << "\n";
}

TEST(DynamicLookup, HotLoopAgreesWithScratchMachine) {
	// Random byte strings through the real match loop, against the from-scratch machine
	for (const auto& lc : all_layouts()) {
		for (const char* name : {"mime_types_120.fix", "lexer_go.fix", "psl_rev_120.fix"}) {
			const auto fxs = dyntest::read_fixtures(data_dir() + "/" + name);
			const auto& fx = fxs.at(0);
			DynamicSet<uint32_t> set(lc.options);
			std::vector<std::pair<uint32_t, const MinimalDfa*>> pats;
			std::vector<MinimalDfa> mins;
			mins.reserve(fx.patterns.size());
			for (const auto& p : fx.patterns) {
				set.add_dfa(p.dfa, p.pid);
				mins.push_back(matcher::minimize_dfa(p.dfa));
			}
			for (size_t i = 0; i < mins.size(); i++) {
				pats.emplace_back(fx.patterns[i].pid, &mins[i]);
			}
			const auto m = dyntest::scratch_product(pats);
			std::mt19937 rng(17);
			for (int n = 0; n < 4000; n++) {
				// walk the scratch machine to build strings that stay live for a while, then random bytes
				std::string s;
				const size_t len = rng() % 24;
				for (size_t i = 0; i < len; i++) {
					s.push_back(static_cast<char>(rng() % 3 == 0 ? rng() % 256 : "abcdefghijklmnop/.-+_"[rng() % 21]));
				}
				if (n % 2 == 0 && !fx.patterns[n % fx.patterns.size()].label.empty() && fx.patterns[0].has_rx) {
					s = fx.patterns[n % fx.patterns.size()].label;  // exact patterns match their own label
				}
				ASSERT_EQ(set.match(s), dyntest::run_machine(m, s)) << lc.name << " " << name;
			}
		}
	}
}

// ---------------------------------------------------------------- edge cases

TEST(DynamicEdge, EdgeCases) {
	for (const auto& lc : all_layouts()) {
		DynamicSet<int> set(lc.options);
		// empty set
		EXPECT_EQ(set.state_count(), 1u);
		EXPECT_TRUE(set.match(std::string("abc")).empty());
		EXPECT_FALSE(set.matches_any(std::string()));
		EXPECT_EQ(set.check_invariants(true), "");
		// empty language: nothing changes
		const auto e = set.add_dfa(dyntest::one_state_dfa(false), 1);
		EXPECT_TRUE(e.empty_language);
		EXPECT_EQ(e.created, 0u);
		EXPECT_EQ(set.state_count(), 1u);
		// empty language whose DFA has two byte classes: tested before REFINE, so no split (proofs ADD
		// step 1; the Python prototype refines first, claims-status F7)
		{
			MinimalDfa empty2 = dyntest::one_state_dfa(false);
			empty2.class_count = 2;
			empty2.byte_class['q'] = 1;
			empty2.delta = {0, 0};
			const size_t k0 = set.class_count();
			const auto e2 = set.add_dfa(empty2, 7);
			EXPECT_TRUE(e2.empty_language);
			EXPECT_EQ(e2.class_splits, 0u);
			EXPECT_EQ(set.class_count(), k0);
			set.remove(7);
		}
		// all strings: no dead state, in every output
		set.add_dfa(dyntest::one_state_dfa(true), 2);
		EXPECT_EQ(set.match(std::string("")), std::vector<int>{2});
		EXPECT_EQ(set.match(std::string("\xff\x00x", 3)), std::vector<int>{2});
		// duplicate language under two ids
		set.add_regex(std::string("ab*"), 3);
		const size_t before = set.state_count();
		set.add_regex(std::string("a(b)*"), 4);
		EXPECT_EQ(set.state_count(), before);
		EXPECT_EQ(set.match(std::string("abb")), (std::vector<int>{2, 3, 4}));
		// a pattern with no dead state
		set.add_dfa(sigma_star_a(), 5);
		EXPECT_EQ(set.match(std::string("xxa")), (std::vector<int>{2, 5}));
		EXPECT_EQ(set.check_invariants(true), "");
		// adding an id twice and removing an unknown id throw
		EXPECT_THROW(set.add_regex(std::string("x"), 3), std::invalid_argument);
		EXPECT_THROW(set.remove(99), std::invalid_argument);
		// remove everything: back to one state
		for (int id : {1, 2, 3, 4, 5}) {
			set.remove(id);
			EXPECT_EQ(set.check_invariants(true), "") << lc.name << " after removing " << id;
		}
		EXPECT_EQ(set.state_count(), 1u);
		EXPECT_EQ(set.pattern_count(), 0u);
		// re-add a removed id
		set.add_regex(std::string("ab*"), 3);
		EXPECT_EQ(set.match(std::string("ab")), std::vector<int>{3});
		EXPECT_EQ(set.check_invariants(true), "");
		// a non-contiguous text
		const std::list<char> text{'a', 'b', 'b'};
		EXPECT_EQ(set.match(text), std::vector<int>{3});
		// a callback that stops
		set.add_regex(std::string("a.*"), 6);
		size_t calls = set.for_each_match(std::string("ab"), [](int) { return false; });
		EXPECT_EQ(calls, 1u);
	}
}

TEST(DynamicEdge, CapLeavesTheSetUnchanged) {
	for (const auto& lc : all_layouts()) {
		DynamicOptions o = lc.options;
		o.max_states = 12;
		DynamicSet<int> set(o);
		set.add_regex(std::string("abc"), 1);
		const size_t before = set.state_count();
		const auto snap = set.snapshot();
		EXPECT_THROW(set.add_regex(std::string("(a|b|c)*a(a|b|c){4}"), 2), std::length_error);
		EXPECT_EQ(set.state_count(), before);
		EXPECT_FALSE(set.contains(2));
		EXPECT_EQ(set.check_invariants(true), "");
		EXPECT_EQ(dyntest::isomorphic(dyntest::from_snapshot(set.snapshot()), dyntest::from_snapshot(snap)), "");
		set.add_regex(std::string("b"), 3);
		EXPECT_EQ(set.match(std::string("b")), std::vector<int>{3});
	}
}

TEST(DynamicEdge, MinimizeAndCompile) {
	// A non-minimal input is minimized; the dead state is found
	MinimalDfa d;
	d.state_count = 4;
	d.class_count = 2;
	for (size_t b = 0; b < 256; b++) {
		d.byte_class[b] = b == 'a' ? 0 : 1;
	}
	// 0 -a-> 1, 0 -a-> 2 duplicates: 1 and 2 accept and go to 3 (dead)
	d.delta = {1, 3, 3, 3, 3, 3, 3, 3};
	d.accepting = {0, 1, 1, 0};
	d.initial = 0;
	const MinimalDfa m = matcher::minimize_dfa(d);
	EXPECT_EQ(m.state_count, 3u);
	EXPECT_NE(m.dead, MinimalDfa::none);
	EXPECT_THROW(matcher::compile_regex_dfa(std::string("a(")), std::invalid_argument);
	const MinimalDfa dot = matcher::compile_regex_dfa(std::string("."));
	EXPECT_EQ(dot.state_count, 3u);  // start, accept, dead
}
