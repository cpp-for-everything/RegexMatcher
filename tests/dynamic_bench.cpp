// DynamicSet benchmark: update time of the arms on the exported pattern sets, lookup time per byte and
// per string, and memory. Provisional workstation numbers (see results-workstation/README.md in p10).
//
//   dynamic_bench <fixture_dir> <out_dir> [--seeds N] [--reps N] [--cap N] [--guard-ms N] [--sets a,b]
//
// Arms, interleaved in a new random order in every repetition:
//   L1     DynamicSet, slot-tagged layout, every add of the order timed
//   L2     DynamicSet, dense layout with spare capacity, every add timed
//   full   full rebuild of D_P for the prefix of the first k patterns (direct product exploration from the
//          initial tuple, keys of live components), timed at the checkpoints k = 1, 2, 4, ... and the last
//   plain  plain product of D_P(k-1) with A_k, reachable part only, timed at the same checkpoints
//          (D_P(k-1) is built untimed by the full rebuild)
//   rxm    RegexMatcher::add_regex of pattern k plus the first lookup (which builds its table), on a
//          matcher that already holds patterns 1..k-1 with its table built (untimed); only for sets where
//          every pattern has regex text that the library accepts
// The A_r of every pattern is given (fixture DFA, minimal). Its build from the regex text is timed
// separately (ar_build.csv) and is not included in any arm.

#include <matcher/core.hpp>
#include <matcher/dynamic.hpp>

#include "dynamic_util.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#endif

namespace {

	using Clock = std::chrono::steady_clock;
	using matcher::DynamicLayout;
	using matcher::DynamicOptions;
	using matcher::DynamicSet;
	using matcher::MinimalDfa;

	volatile uint64_t g_sink = 0;

	int64_t ns_since(Clock::time_point t0) {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count();
	}

	// Nearest rank on the sorted values: v[round(q * (n - 1))]
	double quantile(std::vector<double> v, double q) {
		if (v.empty()) {
			return -1;
		}
		std::sort(v.begin(), v.end());
		return v[static_cast<size_t>(q * double(v.size() - 1) + 0.5)];
	}

	// A product machine over a common byte class partition
	struct Machine {
		uint32_t classes = 0;
		std::array<uint16_t, 256> cls{};
		std::vector<uint32_t> next;  // state * classes + class
		std::vector<uint32_t> out_off{0};
		std::vector<uint32_t> out;   // ids of accepting components, per state
		size_t states() const { return out_off.size() - 1; }
	};

	// Common refinement of a partition with a pattern's classes: new class of byte b from (old, pattern)
	uint32_t refine(std::array<uint16_t, 256>& cls, uint32_t old_k, const MinimalDfa& d, std::vector<uint32_t>& old_of) {
		std::vector<uint32_t> id(size_t(old_k) * d.class_count, 0xFFFFFFFFu);
		old_of.clear();
		uint32_t k = 0;
		for (size_t b = 0; b < 256; b++) {
			uint32_t& e = id[size_t(cls[b]) * d.class_count + d.byte_class[b]];
			if (e == 0xFFFFFFFFu) {
				e = k++;
				old_of.push_back(cls[b]);
			}
			cls[b] = static_cast<uint16_t>(e);
		}
		return k;
	}

	struct FlatKeyTable {
		std::vector<uint32_t> slots;  // state id + 1, 0 empty
		std::vector<uint64_t> hashes;
		size_t mask = 0;
		size_t used = 0;
		explicit FlatKeyTable(size_t cap_log2 = 10) : slots(size_t(1) << cap_log2, 0), mask((size_t(1) << cap_log2) - 1) {}
	};

	uint64_t hash_key(const uint32_t* k, size_t n) {
		uint64_t h = 0x9E3779B97F4A7C15ull ^ n;
		for (size_t i = 0; i < n; i++) {
			h ^= k[i];
			h *= 0xBF58476D1CE4E5B9ull;
			h ^= h >> 29;
		}
		return h;
	}

	// Arm "full": direct exploration of the reachable product of the given automata. A key is the list
	// of (pattern index, component) pairs, dead components left out, stored flat.
	Machine full_rebuild(const std::vector<const MinimalDfa*>& pats, const std::vector<uint32_t>& ids, size_t cap) {
		Machine m;
		std::vector<uint32_t> old_of;
		m.cls.fill(0);
		m.classes = 1;
		for (const auto* d : pats) {
			m.classes = refine(m.cls, m.classes, *d, old_of);
		}
		const uint32_t k = m.classes;
		std::vector<uint8_t> rep(k);
		for (int b = 255; b >= 0; b--) {
			rep[m.cls[b]] = static_cast<uint8_t>(b);
		}
		// per pattern class of each common class
		std::vector<std::vector<uint16_t>> pc(pats.size(), std::vector<uint16_t>(k));
		for (size_t p = 0; p < pats.size(); p++) {
			for (uint32_t c = 0; c < k; c++) {
				pc[p][c] = pats[p]->byte_class[rep[c]];
			}
		}
		std::vector<uint32_t> arena;      // flat keys: p0, s0, p1, s1, ...
		std::vector<uint32_t> key_off{0};
		std::vector<uint64_t> key_hash;
		FlatKeyTable table(12);
		auto find_or_add = [&](const uint32_t* key, size_t n) -> uint32_t {
			const uint64_t h = hash_key(key, n);
			size_t i = h & table.mask;
			while (table.slots[i] != 0) {
				const uint32_t s = table.slots[i] - 1;
				if (key_hash[s] == h && key_off[s + 1] - key_off[s] == n &&
				    std::equal(key, key + n, arena.begin() + key_off[s])) {
					return s;
				}
				i = (i + 1) & table.mask;
			}
			const uint32_t s = static_cast<uint32_t>(key_hash.size());
			if (s >= cap) {
				throw std::length_error("full rebuild past cap");
			}
			arena.insert(arena.end(), key, key + n);
			key_off.push_back(static_cast<uint32_t>(arena.size()));
			key_hash.push_back(h);
			table.slots[i] = s + 1;
			if (++table.used * 2 > table.slots.size()) {
				std::vector<uint32_t> fresh(table.slots.size() * 2, 0);
				const size_t mask = fresh.size() - 1;
				for (uint32_t t = 0; t < key_hash.size(); t++) {
					size_t j = key_hash[t] & mask;
					while (fresh[j] != 0) {
						j = (j + 1) & mask;
					}
					fresh[j] = t + 1;
				}
				table.slots.swap(fresh);
				table.mask = mask;
			}
			return s;
		};
		std::vector<uint32_t> tmp;
		for (uint32_t p = 0; p < pats.size(); p++) {
			if (pats[p]->initial != pats[p]->dead) {
				tmp.push_back(p);
				tmp.push_back(pats[p]->initial);
			}
		}
		find_or_add(tmp.data(), tmp.size());
		for (uint32_t s = 0; s < key_hash.size(); s++) {
			const uint32_t b0 = key_off[s];
			const uint32_t b1 = key_off[s + 1];
			for (uint32_t i = b0; i < b1; i += 2) {
				const MinimalDfa& d = *pats[arena[i]];
				if (d.accepting[arena[i + 1]]) {
					m.out.push_back(ids[arena[i]]);
				}
			}
			m.out_off.push_back(static_cast<uint32_t>(m.out.size()));
			for (uint32_t c = 0; c < k; c++) {
				tmp.clear();
				for (uint32_t i = key_off[s]; i < key_off[s + 1]; i += 2) {
					const uint32_t p = arena[i];
					const MinimalDfa& d = *pats[p];
					const uint32_t t = d.delta[size_t(arena[i + 1]) * d.class_count + pc[p][c]];
					if (t != d.dead) {
						tmp.push_back(p);
						tmp.push_back(t);
					}
				}
				m.next.push_back(find_or_add(tmp.data(), tmp.size()));
			}
		}
		return m;
	}

	// Arm "plain": reachable part of the product of m with a, over the refined class partition
	Machine plain_product(const Machine& m, const MinimalDfa& a, uint32_t id, size_t cap) {
		Machine r;
		r.cls = m.cls;
		std::vector<uint32_t> old_of;
		r.classes = refine(r.cls, m.classes, a, old_of);
		const uint32_t k = r.classes;
		std::vector<uint16_t> acls(k);
		for (int b = 255; b >= 0; b--) {
			acls[r.cls[b]] = a.byte_class[b];
		}
		const size_t ms = m.states();
		const bool flat = ms * a.state_count <= (size_t(1) << 26);
		std::vector<uint32_t> index_flat;
		std::unordered_map<uint64_t, uint32_t> index_map;
		if (flat) {
			index_flat.assign(ms * a.state_count, 0xFFFFFFFFu);
		}
		std::vector<std::pair<uint32_t, uint32_t>> pairs;
		auto find_or_add = [&](uint32_t x, uint32_t q) -> uint32_t {
			uint32_t* slot = nullptr;
			if (flat) {
				slot = &index_flat[size_t(x) * a.state_count + q];
				if (*slot != 0xFFFFFFFFu) {
					return *slot;
				}
			} else {
				const auto it = index_map.find((uint64_t(x) << 32) | q);
				if (it != index_map.end()) {
					return it->second;
				}
			}
			const uint32_t s = static_cast<uint32_t>(pairs.size());
			if (s >= cap) {
				throw std::length_error("plain product past cap");
			}
			pairs.emplace_back(x, q);
			if (flat) {
				*slot = s;
			} else {
				index_map.emplace((uint64_t(x) << 32) | q, s);
			}
			return s;
		};
		find_or_add(0, a.initial);
		r.next.reserve(ms * k);
		for (uint32_t s = 0; s < pairs.size(); s++) {
			const uint32_t x = pairs[s].first;
			const uint32_t q = pairs[s].second;
			r.out.insert(r.out.end(), m.out.begin() + m.out_off[x], m.out.begin() + m.out_off[x + 1]);
			if (a.accepting[q]) {
				r.out.push_back(id);
			}
			r.out_off.push_back(static_cast<uint32_t>(r.out.size()));
			for (uint32_t c = 0; c < k; c++) {
				r.next.push_back(find_or_add(m.next[size_t(x) * m.classes + old_of[c]],
				                             a.delta[size_t(q) * a.class_count + acls[c]]));
			}
		}
		return r;
	}

	struct Paths {
		std::string out_dir;
		std::ofstream ops, checkpoints, lookup, memory, ar_build, cap_log;
	};

	struct SetData {
		dyntest::Fixture fx;
		std::vector<const MinimalDfa*> dfas;  // in file order
		std::vector<uint32_t> ids;
		bool all_rx = false;      // every pattern has regex text that RegexMatcher accepts
		bool std_rx_ok = false;   // and std::regex accepts all of them
	};

	std::vector<size_t> checkpoints_of(size_t n) {
		std::vector<size_t> v;
		for (size_t k = 1; k < n; k *= 2) {
			v.push_back(k);
		}
		v.push_back(n);
		return v;
	}

	struct OpTimes {
		// key: arm, seed, op index -> times over reps
		std::map<std::tuple<std::string, unsigned, size_t>, std::vector<double>> t;
	};

	struct Ctx {
		unsigned seeds = 5;
		unsigned reps = 5;
		size_t cap = 100000;
		int64_t guard_ns = int64_t(3) * 1000 * 1000 * 1000;
		Paths* out = nullptr;
	};

	template <typename F>
	void with_rng_order(std::mt19937_64& rng, std::vector<std::string>& arms, F&& f) {
		std::shuffle(arms.begin(), arms.end(), rng);
		for (const auto& a : arms) {
			f(a);
		}
	}

	struct SetSummary {
		std::vector<double> live_ratio, created_ratio;
		std::map<std::string, std::vector<double>> update_ns;            // arm -> per op median over reps
		std::map<std::string, std::map<size_t, std::vector<double>>> cp_ns;  // arm -> k -> times
		std::map<size_t, std::vector<double>> speedup_full, speedup_plain, speedup_rxm;  // k -> ratios
		size_t cap_hits = 0;
		size_t reached_min = SIZE_MAX;
		size_t final_states_max = 0;
		std::map<std::string, std::map<std::string, double>> lookup_ns_byte, lookup_ns_string;
		std::map<std::string, std::pair<size_t, size_t>> mem;  // engine -> table, aux
	};

	void run_updates(const Ctx& ctx, SetData& sd, SetSummary& sum) {
		const auto& fx = sd.fx;
		const size_t n = sd.dfas.size();
		std::set<std::string> skip;  // "arm:k" cells skipped by the time guard
		for (unsigned seed = 0; seed < ctx.seeds; seed++) {
			std::vector<size_t> order(n);
			std::iota(order.begin(), order.end(), 0);
			std::mt19937_64 orng(0x5EED0000ull + seed);
			std::shuffle(order.begin(), order.end(), orng);
			// reached: how many adds fit under the cap (found by L1 in the first repetition)
			size_t reached = n;
			std::vector<size_t> cps = checkpoints_of(n);
			std::map<std::pair<std::string, size_t>, std::vector<double>> per_op;  // (arm, op) -> times
			std::map<std::pair<std::string, size_t>, std::vector<double>> per_cp;
			std::map<size_t, size_t> states_at;  // op -> states after it (from L1)
			std::vector<std::string> arms{"L1", "L2", "full", "plain"};
			if (sd.all_rx) {
				arms.push_back("rxm");
			}
			std::mt19937_64 arng(0xA2A2ull * 31 + seed);
			for (unsigned rep = 0; rep < ctx.reps; rep++) {
				with_rng_order(arng, arms, [&](const std::string& arm) {
					if (arm == "L1" || arm == "L2") {
						DynamicOptions o;
						o.layout = arm == "L1" ? DynamicLayout::slot_tagged : DynamicLayout::dense_spare;
						o.max_states = ctx.cap;
						o.minimize_input = false;
						DynamicSet<uint32_t> set(o);
						size_t created_sum = 0;
						size_t last_states = 0;
						for (size_t j = 0; j < n; j++) {
							const size_t p = order[j];
							matcher::UpdateStats st;
							const auto t0 = Clock::now();
							try {
								st = set.add_dfa(*sd.dfas[p], sd.ids[p]);
							} catch (const std::length_error&) {
								if (j < reached) {
									reached = j;
								}
								break;
							}
							const int64_t ns = ns_since(t0);
							per_op[{arm, j}].push_back(double(ns));
							if (arm == "L1") {
								states_at[j] = st.states;
							}
							ctx.out->ops << fx.name << ',' << seed << ',' << rep << ',' << arm << ',' << j << ','
							             << sd.ids[p] << ',' << ns << ',' << st.live_region << ',' << st.created << ','
							             << st.deleted << ',' << st.candidates << ',' << st.states << ','
							             << st.classes << ',' << st.class_splits << ',' << st.slot_recycles << ','
							             << st.cells_written << ',' << st.old_rows_written << '\n';
							if (rep == 0 && arm == "L1" && st.states > 0) {
								// live region of the added pattern over |D_P'| after the add
								sum.live_ratio.push_back(double(st.live_region) / double(st.states));
							}
							created_sum += st.created;
							last_states = st.states;
						}
						if (rep == 0 && arm == "L1" && last_states > 0) {
							// states created over the whole build (or up to the cap) over the final |D_P|
							sum.created_ratio.push_back(double(created_sum) / double(last_states));
						}
						return;
					}
					for (const size_t k : cps) {
						if (k > reached) {
							break;
						}
						const std::string cell = arm + ":" + std::to_string(k);
						if (skip.count(cell)) {
							ctx.out->checkpoints << fx.name << ',' << seed << ',' << rep << ',' << arm << ',' << k
							                     << ",,,skipped_time_guard\n";
							continue;
						}
						std::vector<const MinimalDfa*> pre;
						std::vector<uint32_t> pre_ids;
						for (size_t j = 0; j < k; j++) {
							pre.push_back(sd.dfas[order[j]]);
							pre_ids.push_back(sd.ids[order[j]]);
						}
						int64_t ns = -1;
						size_t states = 0;
						std::string status = "ok";
						const auto setup0 = Clock::now();
						try {
							if (arm == "full") {
								const auto t0 = Clock::now();
								const Machine m = full_rebuild(pre, pre_ids, ctx.cap);
								ns = ns_since(t0);
								states = m.states();
							} else if (arm == "plain") {
								std::vector<const MinimalDfa*> before(pre.begin(), pre.end() - 1);
								std::vector<uint32_t> before_ids(pre_ids.begin(), pre_ids.end() - 1);
								const Machine m0 = full_rebuild(before, before_ids, ctx.cap);
								const auto t0 = Clock::now();
								const Machine m = plain_product(m0, *pre.back(), pre_ids.back(), ctx.cap);
								ns = ns_since(t0);
								states = m.states();
							} else {
								matcher::RegexMatcher<uint32_t> rm;
								const auto& pats = fx.patterns;
								std::unordered_map<uint32_t, size_t> at;
								for (size_t i = 0; i < pats.size(); i++) {
									at[pats[i].pid] = i;
								}
								for (size_t j = 0; j + 1 < k; j++) {
									rm.add_regex(pats[at[pre_ids[j]]].rx, pre_ids[j]);
								}
								rm.compile();
								const std::string probe = "x";
								const auto t0 = Clock::now();
								rm.add_regex(pats[at[pre_ids.back()]].rx, pre_ids.back());
								const auto r = rm.match(probe);
								ns = ns_since(t0);
								g_sink += r.size();
								states = rm.table_stats().states;
								if (rm.table_stats().mode != matcher::TableMode::full) {
									status = "table_not_full";
								}
							}
						} catch (const std::length_error&) {
							status = "cap";
						}
						const int64_t total = ns_since(setup0);
						if (status == "ok" && arm != "rxm" && states_at.count(k - 1) && states_at[k - 1] != states) {
							status = "STATE_COUNT_DIFFERS_FROM_L1";
						}
						if (ns >= 0) {
							per_cp[{arm, k}].push_back(double(ns));
						}
						ctx.out->checkpoints << fx.name << ',' << seed << ',' << rep << ',' << arm << ',' << k << ','
						                     << (ns >= 0 ? std::to_string(ns) : std::string()) << ',' << states << ','
						                     << status << '\n';
						if (total > ctx.guard_ns) {
							// later (larger) checkpoints of this arm are skipped from now on; past 4 times the
							// guard, this checkpoint too
							for (const size_t k2 : cps) {
								if (k2 > k || (k2 == k && total > 4 * ctx.guard_ns)) {
									skip.insert(arm + ":" + std::to_string(k2));
								}
							}
						}
					}
				});
			}
			if (reached < n) {
				sum.cap_hits++;
				ctx.out->cap_log << fx.name << ',' << seed << ',' << reached << ',' << n << '\n';
			}
			sum.reached_min = std::min(sum.reached_min, reached);
			if (reached > 0 && states_at.count(reached - 1)) {
				sum.final_states_max = std::max(sum.final_states_max, states_at[reached - 1]);
			}
			for (auto& e : per_op) {
				sum.update_ns[e.first.first].push_back(quantile(e.second, 0.5));
			}
			for (auto& e : per_cp) {
				const double med = quantile(e.second, 0.5);
				sum.cp_ns[e.first.first][e.first.second].push_back(med);
				const size_t k = e.first.second;
				const auto l1 = per_op.find({"L1", k - 1});
				if (l1 != per_op.end()) {
					const double base = quantile(l1->second, 0.5);
					if (base > 0) {
						auto& tgt = e.first.first == "full"    ? sum.speedup_full
						            : e.first.first == "plain" ? sum.speedup_plain
						                                       : sum.speedup_rxm;
						tgt[k].push_back(med / base);
					}
				}
			}
		}
	}

	// Strings for the lookups: matching ones (a random walk in a random pattern, then a shortest path to an
	// accepting state), near misses (a matching string with its last byte changed) and random printable text
	struct Inputs {
		std::vector<std::string> matching, near_miss, non_matching;
	};

	Inputs make_inputs(const SetData& sd, size_t count, uint64_t seed) {
		std::mt19937_64 rng(seed);
		Inputs in;
		const size_t np = sd.dfas.size();
		// distance to acceptance per pattern
		std::vector<std::vector<uint32_t>> dist(np);
		std::vector<std::vector<std::vector<uint8_t>>> class_bytes(np);
		for (size_t p = 0; p < np; p++) {
			const MinimalDfa& d = *sd.dfas[p];
			class_bytes[p].resize(d.class_count);
			for (size_t b = 0; b < 256; b++) {
				class_bytes[p][d.byte_class[b]].push_back(static_cast<uint8_t>(b));
			}
			auto& ds = dist[p];
			ds.assign(d.state_count, 0xFFFFFFFFu);
			std::vector<uint32_t> q;
			for (uint32_t s = 0; s < d.state_count; s++) {
				if (d.accepting[s]) {
					ds[s] = 0;
					q.push_back(s);
				}
			}
			for (size_t h = 0; h < q.size(); h++) {
				for (uint32_t s = 0; s < d.state_count; s++) {
					if (ds[s] != 0xFFFFFFFFu) {
						continue;
					}
					for (uint32_t c = 0; c < d.class_count; c++) {
						if (d.delta[size_t(s) * d.class_count + c] == q[h]) {
							ds[s] = ds[q[h]] + 1;
							q.push_back(s);
							break;
						}
					}
				}
			}
		}
		auto pick_byte = [&](size_t p, uint32_t c) {
			const auto& v = class_bytes[p][c];
			// prefer printable bytes so that the strings look like text
			std::vector<uint8_t> pr;
			for (const uint8_t b : v) {
				if (b >= 32 && b < 127) {
					pr.push_back(b);
				}
			}
			const auto& u = pr.empty() ? v : pr;
			return u[rng() % u.size()];
		};
		size_t tries = 0;
		while (in.matching.size() < count && tries < count * 20) {
			tries++;
			const size_t p = rng() % np;
			const MinimalDfa& d = *sd.dfas[p];
			if (dist[p][d.initial] == 0xFFFFFFFFu) {
				continue;  // empty language
			}
			std::string s;
			uint32_t st = d.initial;
			const size_t walk = rng() % 24;
			for (size_t i = 0; i < walk; i++) {
				std::vector<uint32_t> ok;
				for (uint32_t c = 0; c < d.class_count; c++) {
					if (dist[p][d.delta[size_t(st) * d.class_count + c]] != 0xFFFFFFFFu) {
						ok.push_back(c);
					}
				}
				if (ok.empty()) {
					break;  // an accepting state with no way on
				}
				const uint32_t c = ok[rng() % ok.size()];
				s.push_back(static_cast<char>(pick_byte(p, c)));
				st = d.delta[size_t(st) * d.class_count + c];
			}
			while (dist[p][st] > 0) {
				for (uint32_t c = 0; c < d.class_count; c++) {
					const uint32_t t = d.delta[size_t(st) * d.class_count + c];
					if (dist[p][t] + 1 == dist[p][st]) {
						s.push_back(static_cast<char>(pick_byte(p, c)));
						st = t;
						break;
					}
				}
			}
			in.matching.push_back(s);
			std::string nm = s;
			if (nm.empty()) {
				nm = "~";
			} else {
				char& last = nm.back();
				last = last == '~' ? '!' : '~';
			}
			in.near_miss.push_back(nm);
			std::string r;
			const size_t len = s.size();
			for (size_t i = 0; i < len; i++) {
				r.push_back(static_cast<char>(32 + rng() % 95));
			}
			in.non_matching.push_back(r);
		}
		return in;
	}

	void run_lookups(const Ctx& ctx, SetData& sd, SetSummary& sum) {
		const auto& fx = sd.fx;
		const size_t n = sd.dfas.size();
		// the full set in file order, or its prefix that fits under the cap
		std::vector<std::unique_ptr<DynamicSet<uint32_t>>> sets;
		const DynamicLayout layouts[3] = {DynamicLayout::slot_tagged, DynamicLayout::dense_spare, DynamicLayout::full_bytes};
		const char* names[3] = {"L1", "L2", "L3"};
		size_t used = n;
		for (int l = 0; l < 3; l++) {
			DynamicOptions o;
			o.layout = layouts[l];
			o.max_states = ctx.cap;
			o.minimize_input = false;
			auto s = std::make_unique<DynamicSet<uint32_t>>(o);
			for (size_t p = 0; p < used; p++) {
				try {
					s->add_dfa(*sd.dfas[p], sd.ids[p]);
				} catch (const std::length_error&) {
					used = p;
					break;
				}
			}
			sets.push_back(std::move(s));
		}
		// a set that hit the cap: rebuild every layout on the same prefix
		for (int l = 0; l < 3; l++) {
			if (sets[l]->pattern_count() != used) {
				DynamicOptions o = sets[l]->options();
				auto s = std::make_unique<DynamicSet<uint32_t>>(o);
				for (size_t p = 0; p < used; p++) {
					s->add_dfa(*sd.dfas[p], sd.ids[p]);
				}
				sets[l] = std::move(s);
			}
		}
		std::unique_ptr<matcher::RegexMatcher<uint32_t>> rm;
		std::vector<std::regex> srx;
		if (sd.all_rx) {
			rm = std::make_unique<matcher::RegexMatcher<uint32_t>>();
			for (size_t p = 0; p < used; p++) {
				rm->add_regex(fx.patterns[p].rx, sd.ids[p]);
			}
			rm->compile();
			if (sd.std_rx_ok) {
				for (size_t p = 0; p < used; p++) {
					srx.emplace_back(fx.patterns[p].rx, std::regex::ECMAScript | std::regex::optimize);
				}
			}
		}
		// memory
		for (int l = 0; l < 3; l++) {
			const auto m = sets[l]->memory();
			ctx.out->memory << fx.name << ',' << names[l] << ',' << used << ',' << sets[l]->state_count() << ','
			                << sets[l]->class_count() << ',' << m.table_bytes << ',' << m.table_live_bytes << ','
			                << m.aux_bytes() << ',' << m.key_bytes << ',' << m.index_bytes << ',' << m.live_set_bytes
			                << ',' << m.counter_bytes << ',' << m.output_bytes << ',' << m.other_bytes << ','
			                << m.pattern_bytes << '\n';
			sum.mem[names[l]] = {m.table_bytes, m.aux_bytes()};
		}
		if (rm) {
			const auto ts = rm->table_stats();
			ctx.out->memory << fx.name << ",rxm," << used << ',' << ts.states << ',' << ts.byte_classes << ','
			                << ts.table_bytes << ",," << (ts.total_bytes - ts.table_bytes) << ",,,,,,,\n";
			sum.mem["rxm"] = {ts.table_bytes, ts.total_bytes - ts.table_bytes};
		}
		SetData sub = sd;
		sub.dfas.resize(used);
		sub.ids.resize(used);
		const Inputs in = make_inputs(sub, 1000, 0x1A2B3C);
		const std::vector<std::pair<std::string, const std::vector<std::string>*>> kinds{
		    {"matching", &in.matching}, {"near_miss", &in.near_miss}, {"non_matching", &in.non_matching}};
		std::vector<std::string> engines{"L1", "L2", "L3"};
		if (rm) {
			engines.push_back("rxm");
			if (!srx.empty()) {
				engines.push_back("std_regex");
			}
		}
		// agreement check of the three layouts on every input (not timed)
		size_t disagree = 0;
		for (const auto& k : kinds) {
			for (const auto& s : *k.second) {
				const auto a = sets[0]->match(s);
				if (sets[1]->match(s) != a || sets[2]->match(s) != a) {
					disagree++;
				}
			}
		}
		std::map<std::pair<std::string, std::string>, std::vector<double>> t_byte, t_str;
		std::mt19937_64 rng(0xBEEF);
		std::vector<uint32_t> buf;
		for (unsigned rep = 0; rep < ctx.reps; rep++) {
			std::shuffle(engines.begin(), engines.end(), rng);
			for (const auto& e : engines) {
				for (const auto& k : kinds) {
					const auto& v = *k.second;
					size_t bytes = 0;
					for (const auto& s : v) {
						bytes += s.size();
					}
					// std::regex is slow: a tenth of the strings
					const size_t count = e == "std_regex" ? std::max<size_t>(1, v.size() / 10) : v.size();
					size_t sbytes = 0;
					for (size_t i = 0; i < count; i++) {
						sbytes += v[i].size();
					}
					uint64_t check = 0;
					const auto t0 = Clock::now();
					if (e == "L1" || e == "L2" || e == "L3") {
						const auto& set = *sets[e == "L1" ? 0 : e == "L2" ? 1 : 2];
						for (size_t i = 0; i < count; i++) {
							check += set.match_into(v[i], buf);
						}
					} else if (e == "rxm") {
						for (size_t i = 0; i < count; i++) {
							check += rm->match_into(v[i], buf);
						}
					} else {
						for (size_t i = 0; i < count; i++) {
							buf.clear();
							for (size_t p = 0; p < srx.size(); p++) {
								if (std::regex_match(v[i], srx[p])) {
									buf.push_back(sd.ids[p]);
								}
							}
							check += buf.size();
						}
					}
					const int64_t ns = ns_since(t0);
					g_sink += check;
					const double per_byte = sbytes ? double(ns) / double(sbytes) : -1;
					const double per_str = double(ns) / double(count);
					t_byte[{e, k.first}].push_back(per_byte);
					t_str[{e, k.first}].push_back(per_str);
					ctx.out->lookup << fx.name << ',' << e << ',' << k.first << ',' << rep << ',' << count << ','
					                << sbytes << ',' << ns << ',' << per_byte << ',' << per_str << ',' << check << ','
					                << disagree << '\n';
				}
			}
		}
		for (auto& e : t_byte) {
			sum.lookup_ns_byte[e.first.first][e.first.second] = quantile(e.second, 0.5);
		}
		for (auto& e : t_str) {
			sum.lookup_ns_string[e.first.first][e.first.second] = quantile(e.second, 0.5);
		}
	}

	std::string jnum(double v) {
		if (v < 0) {
			return "null";
		}
		std::ostringstream o;
		o.precision(6);
		o << v;
		return o.str();
	}

	void write_summary(const std::string& path, const std::map<std::string, SetSummary>& all, const Ctx& ctx,
	                   const std::map<std::string, size_t>& patterns_of, int64_t clock_res_ns) {
		std::ofstream o(path, std::ios::binary);
		o << "{\n  \"provisional\": \"workstation numbers, not laptop campaign results\",\n";
		o << "  \"seeds\": " << ctx.seeds << ", \"reps\": " << ctx.reps << ", \"state_cap\": " << ctx.cap
		  << ", \"clock_resolution_ns\": " << clock_res_ns << ",\n";
		o << "  \"quantiles\": \"nearest rank: sorted v[round(q*(n-1))]\",\n  \"sets\": {\n";
		bool first = true;
		for (const auto& e : all) {
			const SetSummary& s = e.second;
			if (!first) {
				o << ",\n";
			}
			first = false;
			o << "    \"" << e.first << "\": {\n";
			o << "      \"patterns\": " << patterns_of.at(e.first) << ", \"seeds_hitting_cap\": " << s.cap_hits
			  << ", \"adds_reached_min\": " << (s.reached_min == SIZE_MAX ? 0 : s.reached_min)
			  << ", \"final_states_max\": " << s.final_states_max << ",\n";
			o << "      \"live_region_ratio\": {\"median\": " << jnum(quantile(s.live_ratio, 0.5))
			  << ", \"p90\": " << jnum(quantile(s.live_ratio, 0.9)) << "},\n";
			o << "      \"created_over_final\": {\"median\": " << jnum(quantile(s.created_ratio, 0.5))
			  << ", \"p90\": " << jnum(quantile(s.created_ratio, 0.9)) << "},\n";
			o << "      \"update_ns\": {";
			bool f2 = true;
			for (const auto& u : s.update_ns) {
				o << (f2 ? "" : ", ") << "\"" << u.first << "\": {\"median\": " << jnum(quantile(u.second, 0.5))
				  << ", \"p90\": " << jnum(quantile(u.second, 0.9)) << "}";
				f2 = false;
			}
			o << "},\n      \"checkpoint_ns_median\": {";
			f2 = true;
			for (const auto& a : s.cp_ns) {
				o << (f2 ? "" : ", ") << "\"" << a.first << "\": {";
				bool f3 = true;
				for (const auto& k : a.second) {
					o << (f3 ? "" : ", ") << "\"" << k.first << "\": " << jnum(quantile(k.second, 0.5));
					f3 = false;
				}
				o << "}";
				f2 = false;
			}
			o << "},\n";
			auto speed = [&](const char* name, const std::map<size_t, std::vector<double>>& m, bool last) {
				o << "      \"" << name << "\": {";
				bool f = true;
				for (const auto& k : m) {
					o << (f ? "" : ", ") << "\"" << k.first << "\": {\"median\": " << jnum(quantile(k.second, 0.5))
					  << ", \"min\": " << jnum(quantile(k.second, 0.0)) << ", \"max\": " << jnum(quantile(k.second, 1.0))
					  << "}";
					f = false;
				}
				o << "}" << (last ? "\n" : ",\n");
			};
			speed("speedup_L1_vs_full_by_prefix", s.speedup_full, false);
			speed("speedup_L1_vs_plain_by_prefix", s.speedup_plain, false);
			speed("speedup_L1_vs_rxm_by_prefix", s.speedup_rxm, false);
			o << "      \"lookup_ns_per_byte\": {";
			f2 = true;
			for (const auto& en : s.lookup_ns_byte) {
				o << (f2 ? "" : ", ") << "\"" << en.first << "\": {";
				bool f3 = true;
				for (const auto& k : en.second) {
					o << (f3 ? "" : ", ") << "\"" << k.first << "\": " << jnum(k.second);
					f3 = false;
				}
				o << "}";
				f2 = false;
			}
			o << "},\n      \"lookup_ns_per_string\": {";
			f2 = true;
			for (const auto& en : s.lookup_ns_string) {
				o << (f2 ? "" : ", ") << "\"" << en.first << "\": {";
				bool f3 = true;
				for (const auto& k : en.second) {
					o << (f3 ? "" : ", ") << "\"" << k.first << "\": " << jnum(k.second);
					f3 = false;
				}
				o << "}";
				f2 = false;
			}
			o << "},\n      \"memory_bytes\": {";
			f2 = true;
			for (const auto& m : s.mem) {
				o << (f2 ? "" : ", ") << "\"" << m.first << "\": {\"table\": " << m.second.first
				  << ", \"aux\": " << m.second.second << "}";
				f2 = false;
			}
			o << "}\n    }";
		}
		o << "\n  }\n}\n";
	}

	int64_t clock_resolution() {
		int64_t best = INT64_MAX;
		for (int i = 0; i < 1000; i++) {
			const auto a = Clock::now();
			auto b = Clock::now();
			while (b == a) {
				b = Clock::now();
			}
			best = std::min<int64_t>(best, std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count());
		}
		return best;
	}

}  // namespace

int main(int argc, char** argv) {
	if (argc < 3) {
		std::cerr << "usage: dynamic_bench <fixture_dir> <out_dir> [--seeds N] [--reps N] [--cap N] [--guard-ms N] [--sets a,b]\n";
		return 2;
	}
	const std::string fdir = argv[1];
	Paths out;
	out.out_dir = argv[2];
	Ctx ctx;
	ctx.out = &out;
	std::set<std::string> only;
	for (int i = 3; i + 1 < argc; i += 2) {
		const std::string a = argv[i];
		const std::string v = argv[i + 1];
		if (a == "--seeds") {
			ctx.seeds = unsigned(std::stoul(v));
		} else if (a == "--reps") {
			ctx.reps = unsigned(std::stoul(v));
		} else if (a == "--cap") {
			ctx.cap = std::stoull(v);
		} else if (a == "--guard-ms") {
			ctx.guard_ns = int64_t(std::stoll(v)) * 1000 * 1000;
		} else if (a == "--sets") {
			std::stringstream ss(v);
			std::string t;
			while (std::getline(ss, t, ',')) {
				only.insert(t);
			}
		}
	}
	const std::vector<std::string> files{
	    "adversarial_family_1_6", "email_group_0", "gitignore_templates_rev_sample", "lexer_c", "lexer_c_python_go",
	    "lexer_go", "mime_types", "mime_types_120", "mqtt_synthetic_1000", "mqtt_synthetic_80", "psl_1000_rev",
	    "psl_rev_120", "regexlib_email_all", "routes_github", "routes_github_search", "routes_gplus", "routes_parse",
	    "routes_static", "validators_mixed", "validators_mixed_20", "words_100", "words_1000"};
	out.ops.open(out.out_dir + "/update_ops.csv", std::ios::binary);
	out.ops << "set,seed,rep,arm,op,id,ns,live_region,created,deleted,candidates,states,classes,class_splits,"
	           "slot_recycles,cells_written,old_rows_written\n";
	out.checkpoints.open(out.out_dir + "/checkpoints.csv", std::ios::binary);
	out.checkpoints << "set,seed,rep,arm,prefix,ns,states,status\n";
	out.lookup.open(out.out_dir + "/lookup.csv", std::ios::binary);
	out.lookup << "set,engine,kind,rep,strings,bytes,ns,ns_per_byte,ns_per_string,checksum,layout_disagreements\n";
	out.memory.open(out.out_dir + "/memory.csv", std::ios::binary);
	out.memory << "set,engine,patterns,states,classes,table_bytes,table_live_bytes,aux_bytes,key_bytes,index_bytes,"
	              "live_set_bytes,counter_bytes,output_bytes,other_bytes,pattern_bytes\n";
	out.ar_build.open(out.out_dir + "/ar_build.csv", std::ios::binary);
	out.ar_build << "set,id,ns_median,reps,states,classes,status\n";
	out.cap_log.open(out.out_dir + "/cap_hits.csv", std::ios::binary);
	out.cap_log << "set,seed,adds_reached,patterns\n";
	if (!out.ops || !out.checkpoints || !out.lookup || !out.memory || !out.ar_build || !out.cap_log) {
		std::cerr << "cannot write to " << out.out_dir << "\n";
		return 1;
	}
	const int64_t res = clock_resolution();
	std::map<std::string, SetSummary> all;
	std::map<std::string, size_t> patterns_of;
	for (const auto& f : files) {
		if (!only.empty() && !only.count(f)) {
			continue;
		}
		auto fxs = dyntest::read_fixtures(fdir + "/" + f + ".fix");
		for (auto& fx : fxs) {
			SetData sd;
			sd.fx = std::move(fx);
			sd.all_rx = !sd.fx.patterns.empty();
			sd.std_rx_ok = true;
			for (auto& p : sd.fx.patterns) {
				sd.dfas.push_back(&p.dfa);
				sd.ids.push_back(p.pid);
				if (!p.has_rx) {
					sd.all_rx = false;
				}
			}
			// A_r build from the regex text, timed apart (median of reps); also tells whether the library
			// and std::regex accept every pattern
			for (auto& p : sd.fx.patterns) {
				if (!p.has_rx) {
					continue;
				}
				std::vector<double> t;
				std::string status = "ok";
				uint32_t st = 0, cl = 0;
				for (unsigned r = 0; r < ctx.reps; r++) {
					try {
						const auto t0 = Clock::now();
						const MinimalDfa d = matcher::compile_regex_dfa(p.rx);
						t.push_back(double(ns_since(t0)));
						st = d.state_count;
						cl = d.class_count;
					} catch (const std::length_error&) {
						status = "cap";
						break;
					} catch (const std::exception&) {
						status = "rejected";
						break;
					}
				}
				if (status != "ok") {
					sd.all_rx = false;
				}
				try {
					std::regex probe(p.rx, std::regex::ECMAScript);
				} catch (const std::exception&) {
					sd.std_rx_ok = false;
				}
				out.ar_build << sd.fx.name << ',' << p.pid << ',' << (t.empty() ? std::string() : jnum(quantile(t, 0.5)))
				             << ',' << t.size() << ',' << st << ',' << cl << ',' << status << '\n';
			}
			if (!sd.all_rx) {
				sd.std_rx_ok = false;
			}
			std::cerr << "[bench] " << sd.fx.name << ": " << sd.dfas.size() << " patterns, regex arm "
			          << (sd.all_rx ? "yes" : "no") << ", std::regex " << (sd.std_rx_ok ? "yes" : "no") << std::endl;
			SetSummary& sum = all[sd.fx.name];
			patterns_of[sd.fx.name] = sd.dfas.size();
			const auto t0 = Clock::now();
			run_updates(ctx, sd, sum);
			run_lookups(ctx, sd, sum);
			std::cerr << "[bench] " << sd.fx.name << " done in " << ns_since(t0) / 1000000 << " ms" << std::endl;
			out.ops.flush();
			out.checkpoints.flush();
			out.lookup.flush();
		}
	}
	write_summary(out.out_dir + "/summary.json", all, ctx, patterns_of, res);
	std::cerr << "[bench] clock resolution " << res << " ns, sink " << g_sink << std::endl;
	return 0;
}
