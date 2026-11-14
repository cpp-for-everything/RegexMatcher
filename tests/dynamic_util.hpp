#pragma once
// Helpers for the DynamicSet tests and benchmark: the fixture reader (format written by
// p10-regex-update/prototype/experiments/export_fixtures.py), an independent from-scratch product
// of minimal DFAs, and an isomorphism check of two Moore machines over all 256 bytes.
//
// The from-scratch product does not use ADD or REMOVE: it explores the reachable product from the
// initial tuple. A tuple is stored by its components that are not dead; a dead component stays dead
// (the dead state is absorbing), so it needs no stepping.

#include <matcher/dynamic.hpp>

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dyntest {

	struct FixturePattern {
		uint32_t pid = 0;
		std::string label;
		bool has_rx = false;
		std::string rx;
		uint32_t dead_in_file = matcher::MinimalDfa::none;
		matcher::MinimalDfa dfa;
	};

	struct FixtureOp {
		char kind = 'a';  // 'a' add, 'r' remove
		uint32_t pid = 0;
		size_t states = 0;
		size_t live = 0;
		size_t created = 0;
		size_t deleted = 0;  // garbage on add, deleted on remove
		size_t classes = 0;
		size_t extra = 0;  // candidates on add, merged on remove
	};

	struct Fixture {
		std::string name;
		std::string family;
		std::string status;
		std::vector<FixturePattern> patterns;
		std::vector<FixtureOp> ops;
	};

	// Percent escapes as written by export_fixtures.py; a lone "%" is the empty string. (A missing regex
	// is "-" with the has_rx flag 0; the caller looks at the flag.)
	inline std::string unescape(const std::string& s) {
		std::string out;
		for (size_t i = 0; i < s.size(); i++) {
			if (s[i] == '%') {
				if (i + 2 < s.size()) {
					out.push_back(static_cast<char>(std::stoi(s.substr(i + 1, 2), nullptr, 16)));
					i += 2;
				}
			} else {
				out.push_back(s[i]);
			}
		}
		return out;
	}

	inline std::vector<Fixture> read_fixtures(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) {
			throw std::runtime_error("cannot open fixture file " + path);
		}
		std::vector<Fixture> out;
		std::string line;
		Fixture* cur = nullptr;
		while (std::getline(in, line)) {
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}
			if (line.empty()) {
				continue;
			}
			const size_t sp = line.find(' ');
			const std::string tag = line.substr(0, sp);
			const std::string rest = sp == std::string::npos ? std::string() : line.substr(sp + 1);
			std::istringstream ss(rest);
			if (tag == "set") {
				out.emplace_back();
				cur = &out.back();
				cur->name = rest;
			} else if (tag == "family") {
				cur->family = rest;
			} else if (tag == "pattern") {
				FixturePattern p;
				long long dead = 0;
				int has_rx = 0;
				ss >> p.pid >> p.dfa.state_count >> p.dfa.class_count >> p.dfa.initial >> dead >> has_rx;
				p.dead_in_file = dead < 0 ? matcher::MinimalDfa::none : static_cast<uint32_t>(dead);
				p.dfa.dead = p.dead_in_file;
				p.has_rx = has_rx != 0;
				cur->patterns.push_back(std::move(p));
			} else if (tag == "label") {
				cur->patterns.back().label = unescape(rest);
			} else if (tag == "rx") {
				cur->patterns.back().rx = unescape(rest);
			} else if (tag == "cls") {
				for (size_t b = 0; b < 256; b++) {
					ss >> cur->patterns.back().dfa.byte_class[b];
				}
			} else if (tag == "acc") {
				auto& d = cur->patterns.back().dfa;
				d.accepting.clear();
				for (const char c : rest) {
					d.accepting.push_back(c == '1' ? 1 : 0);
				}
			} else if (tag == "delta") {
				auto& d = cur->patterns.back().dfa;
				d.delta.assign(size_t(d.state_count) * d.class_count, 0);
				for (auto& t : d.delta) {
					ss >> t;
				}
			} else if (tag == "ops") {
				size_t n = 0;
				ss >> n >> cur->status;
				cur->ops.reserve(n);
			} else if (tag == "a" || tag == "r") {
				FixtureOp op;
				op.kind = tag[0];
				ss >> op.pid >> op.states >> op.live >> op.created >> op.deleted >> op.classes >> op.extra;
				cur->ops.push_back(op);
			} else if (tag == "end") {
				cur = nullptr;
			}
		}
		return out;
	}

	// A Moore machine over 256 bytes with sorted id outputs
	template <typename Id>
	struct Machine {
		size_t states = 0;
		uint32_t initial = 0;
		std::vector<uint32_t> next;  // state * 256 + byte
		std::vector<std::vector<Id>> outputs;
	};

	template <typename Id>
	Machine<Id> from_snapshot(const matcher::DynamicSnapshot<Id>& s) {
		Machine<Id> m;
		m.states = s.states;
		m.initial = s.initial;
		m.next = s.next;
		m.outputs = s.outputs;
		return m;
	}

	struct PairVecHash {
		size_t operator()(const std::vector<std::pair<uint32_t, uint32_t>>& v) const noexcept {
			uint64_t h = 1469598103934665603ull;
			for (const auto& e : v) {
				h = (h ^ e.first) * 1099511628211ull;
				h = (h ^ e.second) * 1099511628211ull;
			}
			return static_cast<size_t>(h);
		}
	};

	// The reachable product of the given automata, built from the initial tuple. Throws std::length_error
	// past cap states.
	template <typename Id>
	Machine<Id> scratch_product(const std::vector<std::pair<Id, const matcher::MinimalDfa*>>& pats,
	                            size_t cap = size_t(1) << 22) {
		using Key = std::vector<std::pair<uint32_t, uint32_t>>;  // (index in pats, component), not dead
		// common refinement of the byte partitions: one representative byte per class
		std::map<std::vector<uint16_t>, uint32_t> sig_class;
		std::vector<uint32_t> cls(256);
		std::vector<uint8_t> reps;
		for (size_t b = 0; b < 256; b++) {
			std::vector<uint16_t> sig;
			for (const auto& p : pats) {
				sig.push_back(p.second->byte_class[b]);
			}
			const auto it = sig_class.emplace(sig, static_cast<uint32_t>(reps.size()));
			if (it.second) {
				reps.push_back(static_cast<uint8_t>(b));
			}
			cls[b] = it.first->second;
		}
		const size_t k = reps.size();
		std::unordered_map<Key, uint32_t, PairVecHash> index;
		std::vector<Key> keys;
		Key start;
		for (uint32_t i = 0; i < pats.size(); i++) {
			const auto& d = *pats[i].second;
			if (d.initial != d.dead) {
				start.emplace_back(i, d.initial);
			}
		}
		index.emplace(start, 0);
		keys.push_back(start);
		std::vector<uint32_t> cnext;
		for (size_t cur = 0; cur < keys.size(); cur++) {
			for (size_t c = 0; c < k; c++) {
				Key t;
				for (const auto& e : keys[cur]) {
					const auto& d = *pats[e.first].second;
					const uint32_t s2 = d.next(e.second, reps[c]);
					if (s2 != d.dead) {
						t.emplace_back(e.first, s2);
					}
				}
				auto it = index.find(t);
				uint32_t id = 0;
				if (it == index.end()) {
					if (keys.size() >= cap) {
						throw std::length_error("scratch product past cap");
					}
					id = static_cast<uint32_t>(keys.size());
					index.emplace(t, id);
					keys.push_back(std::move(t));
				} else {
					id = it->second;
				}
				cnext.push_back(id);
			}
		}
		Machine<Id> m;
		m.states = keys.size();
		m.initial = 0;
		m.next.resize(keys.size() * 256);
		m.outputs.resize(keys.size());
		for (size_t s = 0; s < keys.size(); s++) {
			for (size_t b = 0; b < 256; b++) {
				m.next[s * 256 + b] = cnext[s * k + cls[b]];
			}
			for (const auto& e : keys[s]) {
				if (pats[e.first].second->accepting[e.second]) {
					m.outputs[s].push_back(pats[e.first].first);
				}
			}
			std::sort(m.outputs[s].begin(), m.outputs[s].end());
		}
		return m;
	}

	// Empty string when a and b are isomorphic (transition and output preserving bijection that maps the
	// initial state to the initial state), else the reason.
	template <typename Id>
	std::string isomorphic(const Machine<Id>& a, const Machine<Id>& b) {
		if (a.states != b.states) {
			return "state counts differ: " + std::to_string(a.states) + " vs " + std::to_string(b.states);
		}
		std::vector<uint32_t> f(a.states, 0xFFFFFFFFu), g(b.states, 0xFFFFFFFFu);
		std::vector<uint32_t> queue{a.initial};
		f[a.initial] = b.initial;
		g[b.initial] = a.initial;
		for (size_t h = 0; h < queue.size(); h++) {
			const uint32_t x = queue[h];
			const uint32_t y = f[x];
			if (a.outputs[x] != b.outputs[y]) {
				return "outputs differ";
			}
			for (size_t c = 0; c < 256; c++) {
				const uint32_t x2 = a.next[size_t(x) * 256 + c];
				const uint32_t y2 = b.next[size_t(y) * 256 + c];
				if (f[x2] != 0xFFFFFFFFu) {
					if (f[x2] != y2) {
						return "transition mismatch";
					}
				} else {
					if (g[y2] != 0xFFFFFFFFu) {
						return "not injective";
					}
					f[x2] = y2;
					g[y2] = x2;
					queue.push_back(x2);
				}
			}
		}
		if (queue.size() != a.states) {
			return "only " + std::to_string(queue.size()) + " of " + std::to_string(a.states) + " states reachable";
		}
		return std::string();
	}

	template <typename Id>
	std::vector<Id> run_machine(const Machine<Id>& m, const std::string& text) {
		uint32_t s = m.initial;
		for (const char ch : text) {
			s = m.next[size_t(s) * 256 + static_cast<uint8_t>(ch)];
		}
		return m.outputs[s];
	}

	// The one-state DFAs of the empty language and of all strings
	inline matcher::MinimalDfa one_state_dfa(bool accepting) {
		matcher::MinimalDfa d;
		d.state_count = 1;
		d.class_count = 1;
		d.delta = {0};
		d.accepting = {static_cast<uint8_t>(accepting ? 1 : 0)};
		d.initial = 0;
		d.dead = accepting ? matcher::MinimalDfa::none : 0;
		return d;
	}

}  // namespace dyntest
