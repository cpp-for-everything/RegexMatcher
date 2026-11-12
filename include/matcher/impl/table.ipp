/**
 * @file table.ipp
 * @brief Template implementation for RegexMatcher class: the lookup table and the lookup entry points
 * @note This file is included by core.hpp - do not include directly
 *
 * match() and its relatives only ask which regexes match the whole text. For 8 bit char_t they answer it
 * with a deterministic table built from the automaton by subset construction: one table load per byte.
 * The table is built on the first lookup after a change, or by compile(). When the full build would pass
 * its caps or its work bound, the matcher fills a lazy table during lookups instead, and a lookup that
 * would pass the caps there simulates the automaton (see node.ipp). With a char_t wider than 8 bits the
 * matcher always simulates. Every mode gives the same answers. match_with_groups() always simulates, and
 * uses a full table only to return at once when nothing matches.
 */

#include <algorithm>
#include <new>

namespace matcher {

	namespace detail {
		// Bookkeeping bytes counted for one table state besides its row and its set: the hash map node and
		// the list entries. An estimate, used only against the byte cap.
		constexpr size_t table_state_overhead = 64;

		// The full build also stops when the sets of its states hold more automaton states in total than
		// this many per automaton state (or the floor), so a set of patterns whose table explodes falls to
		// the lazy table after work proportional to the automaton, not after the caps.
		constexpr size_t full_build_work_factor = 64;
		constexpr size_t full_build_work_floor = size_t(1) << 18;
	}  // namespace detail

	// Byte classes: one partition of the 256 byte values, refined by each character class. A byte is read
	// as char_t, like the simulation reads it, so ranges of a signed char stay right.
	template <typename RegexData, typename char_t>
	detail::ByteMap RegexMatcher<RegexData, char_t>::byte_map() const {
		detail::ByteMap map;
		std::array<uint16_t, 256> byte_class{};
		size_t class_count = 1;
		std::vector<uint16_t> renumber;
		for (const auto& cls : classes) {
			renumber.assign(2 * class_count, uint16_t(0xFFFF));
			uint16_t next_id = 0;
			for (size_t b = 0; b < 256; b++) {
				const size_t key = 2 * size_t(byte_class[b]) + (cls.contains(static_cast<char_t>(b)) ? 1 : 0);
				if (renumber[key] == 0xFFFF) {
					renumber[key] = next_id++;
				}
				byte_class[b] = renumber[key];
			}
			class_count = next_id;
		}
		std::vector<size_t> representative(class_count, 0);
		for (size_t b = 256; b-- > 0;) {
			representative[byte_class[b]] = b;
		}
		map.accepts.resize(classes.size());
		for (size_t c = 0; c < classes.size(); c++) {
			for (size_t k = 0; k < class_count; k++) {
				if (classes[c].contains(static_cast<char_t>(representative[k]))) {
					map.accepts[c].push_back(static_cast<uint16_t>(k));
				}
			}
		}
		for (size_t b = 0; b < 256; b++) {
			map.byte_class[b] = static_cast<uint8_t>(byte_class[b]);
		}
		map.class_count = class_count;
		while ((size_t(1) << map.shift) < class_count) {
			map.shift++;
		}
		return map;
	}

	// For a lookup without captures every state but Char and Accept is an empty move (Open, Close, Reset,
	// Enter and Check only matter for captures, see run<false>). So a table state is the sorted set of
	// Char and Accept states that the empty moves reach.
	template <typename RegexData, typename char_t>
	void RegexMatcher<RegexData, char_t>::closure(const std::vector<uint32_t>& seeds, std::vector<uint32_t>& set,
	                                              detail::ClosureScratch& scratch) const {
		using Kind = detail::State::Kind;
		if (scratch.mark.size() < states.size()) {
			scratch.mark.resize(states.size(), 0);
		}
		const uint64_t stamp = ++scratch.stamp;
		auto& stack = scratch.stack;
		set.clear();
		stack.assign(seeds.rbegin(), seeds.rend());
		while (!stack.empty()) {
			const uint32_t s = stack.back();
			stack.pop_back();
			if (scratch.mark[s] == stamp) {
				continue;
			}
			scratch.mark[s] = stamp;
			const detail::State& state = states[s];
			switch (state.kind) {
				case Kind::Char:
				case Kind::Accept:
					set.push_back(s);
					break;
				case Kind::Split:
					stack.insert(stack.end(), state.out.rbegin(), state.out.rend());
					break;
				default:
					stack.push_back(state.out[0]);
					break;
			}
		}
		std::sort(set.begin(), set.end());
	}

	// The states that the Char states of `set` move to on a byte of `byte_class`
	template <typename RegexData, typename char_t>
	void RegexMatcher<RegexData, char_t>::seeds_for(const std::vector<uint32_t>& set, const detail::ByteMap& map,
	                                                size_t byte_class, std::vector<uint32_t>& seeds) const {
		seeds.clear();
		for (const uint32_t s : set) {
			const detail::State& state = states[s];
			if (state.kind != detail::State::Kind::Char) {
				continue;
			}
			const auto& accepts = map.accepts[state.value];
			if (std::find(accepts.begin(), accepts.end(), static_cast<uint16_t>(byte_class)) != accepts.end()) {
				seeds.push_back(state.out[0]);
			}
		}
	}

	// Appends the ids that `set` accepts, sorted, each once
	template <typename RegexData, typename char_t>
	void RegexMatcher<RegexData, char_t>::accepted_ids(const std::vector<uint32_t>& set,
	                                                   std::vector<RegexData>& out) const {
		const size_t first = out.size();
		for (const uint32_t n : set) {
			if (states[n].kind == detail::State::Kind::Accept) {
				out.push_back(accept_ids[states[n].value]);
			}
		}
		const auto begin = out.begin() + static_cast<std::ptrdiff_t>(first);
		std::sort(begin, out.end(), [](const RegexData& a, const RegexData& b) { return a < b; });
		out.erase(std::unique(out.begin() + static_cast<std::ptrdiff_t>(first), out.end(),
		                      [](const RegexData& a, const RegexData& b) { return !(a < b) && !(b < a); }),
		          out.end());
	}

	// Subset construction of the whole table. Returns null when it finished, or which bound stopped it.
	template <typename RegexData, typename char_t>
	const char* RegexMatcher<RegexData, char_t>::fill_table(const detail::ByteMap& map,
	                                                        detail::LookupTable<RegexData>& out) const {
		using Kind = detail::State::Kind;
		const size_t class_count = map.class_count;
		const uint32_t shift = map.shift;
		const size_t stride = size_t(1) << shift;
		const size_t work_bound =
		    std::max(detail::full_build_work_floor, detail::full_build_work_factor * states.size());

		detail::ClosureScratch scratch;
		std::unordered_map<std::vector<uint32_t>, uint32_t, detail::StateSetHash> index;
		std::vector<const std::vector<uint32_t>*> sets;  // the set of each table state, keys of `index`
		std::vector<uint32_t> transitions;                 // state * class_count + class -> target state
		size_t bytes = 0;
		size_t work = 0;
		const char* stopped = nullptr;
		const auto add_state = [&](std::vector<uint32_t>&& set, uint32_t& id) {
			const auto found = index.find(set);
			if (found != index.end()) {
				id = found->second;
				return true;
			}
			const size_t added = stride * sizeof(uint32_t) + set.size() * sizeof(uint32_t) + detail::table_state_overhead;
			if (sets.size() + 1 > limits.max_states || sets.size() >= (size_t(1) << 31)) {
				stopped = "states";
			} else if (bytes + added > limits.max_bytes) {
				stopped = "bytes";
			} else if (work + set.size() > work_bound) {
				stopped = "work";
			}
			if (stopped != nullptr) {
				out.stats.full_states_reached = sets.size();
				return false;
			}
			bytes += added;
			work += set.size();
			id = static_cast<uint32_t>(sets.size());
			const auto inserted = index.emplace(std::move(set), id).first;
			sets.push_back(&inserted->first);
			transitions.resize(transitions.size() + class_count, 0);
			return true;
		};

		uint32_t id = 0;
		if (!add_state({}, id)) {  // the dead state, 0
			return stopped;
		}
		std::vector<uint32_t> set;
		if (!states.empty()) {
			closure({0}, set, scratch);
		}
		uint32_t start = 0;
		if (!add_state(std::move(set), start)) {
			return stopped;
		}
		std::vector<std::vector<uint32_t>> buckets(class_count);
		for (size_t current = 1; current < sets.size(); current++) {
			for (auto& bucket : buckets) {
				bucket.clear();
			}
			for (const uint32_t s : *sets[current]) {
				const detail::State& state = states[s];
				if (state.kind != Kind::Char) {
					continue;
				}
				for (const uint16_t k : map.accepts[state.value]) {
					buckets[k].push_back(state.out[0]);
				}
			}
			for (size_t k = 0; k < class_count; k++) {
				uint32_t target = 0;
				if (!buckets[k].empty()) {
					closure(buckets[k], set, scratch);
					if (!add_state(std::move(set), target)) {
						return stopped;
					}
				}
				transitions[current * class_count + k] = target;
			}
		}

		// The final layout: rows of `stride` entries, each entry the target row
		const size_t count = sets.size();
		const size_t entries = count * stride;
		if (static_cast<uint64_t>(entries) > (uint64_t(1) << 32)) {
			out.stats.full_states_reached = count;
			return "states";
		}
		const auto fill = [&](auto& table) {
			using Entry = typename std::remove_reference_t<decltype(table)>::value_type;
			table.assign(entries, 0);
			for (size_t s = 0; s < count; s++) {
				for (size_t k = 0; k < class_count; k++) {
					table[(s << shift) + k] = static_cast<Entry>(size_t(transitions[s * class_count + k]) << shift);
				}
			}
		};
		if (entries <= (size_t(1) << 16)) {
			fill(out.narrow);
		} else {
			fill(out.wide);
		}
		transitions = {};

		out.accept_begin.reserve(count + 1);
		out.accept_begin.push_back(0);
		for (size_t s = 0; s < count; s++) {
			accepted_ids(*sets[s], out.accepted);
			out.accept_begin.push_back(static_cast<uint32_t>(out.accepted.size()));
		}
		out.byte_class = map.byte_class;
		out.shift = shift;
		out.start = start << shift;
		out.mode = TableMode::full;
		out.stats.states = count;
		out.stats.byte_classes = class_count;
		out.stats.table_bytes = out.narrow.size() * sizeof(uint16_t) + out.wide.size() * sizeof(uint32_t);
		out.stats.total_bytes = out.stats.table_bytes + sizeof(out.byte_class) +
		                        out.accept_begin.size() * sizeof(uint32_t) + out.accepted.size() * sizeof(RegexData);
		out.stats.full_states_reached = count;
		return nullptr;
	}

	// The lazy table with its dead state and its start state. False when even these pass the caps.
	template <typename RegexData, typename char_t>
	bool RegexMatcher<RegexData, char_t>::start_lazy(const detail::ByteMap& map,
	                                                 detail::LookupTable<RegexData>& out) const {
		auto lazy = std::make_unique<detail::LazyTable<RegexData>>();
		lazy->map = map;
		lazy->max_states = limits.max_states;
		lazy->max_bytes = limits.max_bytes;
		std::vector<uint32_t> start;
		if (!states.empty()) {
			closure({0}, start, lazy->closure);
		}
		const bool start_is_dead = start.empty();
		const size_t needed = start_is_dead ? 1 : 2;
		const size_t row = map.class_count * sizeof(uint32_t) + detail::table_state_overhead;
		if (needed > limits.max_states || needed * row + start.size() * sizeof(uint32_t) > limits.max_bytes) {
			return false;
		}
		lazy->accept_begin.push_back(0);
		const auto add = [&](std::vector<uint32_t>&& set, uint32_t fill) {
			const uint32_t id = static_cast<uint32_t>(lazy->sets.size());
			lazy->bytes += row + set.size() * sizeof(uint32_t);
			const auto inserted = lazy->index.emplace(std::move(set), id).first;
			lazy->sets.push_back(&inserted->first);
			lazy->next.resize(lazy->next.size() + map.class_count, fill);
			accepted_ids(inserted->first, lazy->accepted);
			lazy->accept_begin.push_back(static_cast<uint32_t>(lazy->accepted.size()));
			return id;
		};
		add({}, 0);  // the dead state: every move stays dead
		lazy->start = start_is_dead ? 0 : add(std::move(start), detail::LazyTable<RegexData>::unknown);
		out.lazy = std::move(lazy);
		out.mode = TableMode::lazy;
		out.stats.byte_classes = map.class_count;
		return true;
	}

	// Computes one missing move of the lazy table; the caller holds its mutex. Returns the target, or
	// unknown when a new state would pass the caps (the table is left as it was).
	template <typename RegexData, typename char_t>
	uint32_t RegexMatcher<RegexData, char_t>::lazy_add(detail::LazyTable<RegexData>& lazy, uint32_t state,
	                                                   uint32_t byte_class) const {
		constexpr uint32_t unknown = detail::LazyTable<RegexData>::unknown;
		const size_t slot = size_t(state) * lazy.map.class_count + byte_class;
		try {
			seeds_for(*lazy.sets[state], lazy.map, byte_class, lazy.seeds);
			if (lazy.seeds.empty()) {
				lazy.next[slot] = 0;
				return 0;
			}
			std::vector<uint32_t> set;
			closure(lazy.seeds, set, lazy.closure);
			const auto found = lazy.index.find(set);
			if (found != lazy.index.end()) {
				lazy.next[slot] = found->second;
				return found->second;
			}
			const size_t added = lazy.map.class_count * sizeof(uint32_t) + set.size() * sizeof(uint32_t) +
			                     detail::table_state_overhead;
			if (lazy.sets.size() + 1 > lazy.max_states || lazy.sets.size() >= (size_t(1) << 31) ||
			    lazy.bytes + added > lazy.max_bytes) {
				return unknown;
			}
			// Everything that allocates comes before the first change
			std::vector<RegexData> ids;
			accepted_ids(set, ids);
			lazy.next.reserve(lazy.next.size() + lazy.map.class_count);
			lazy.sets.reserve(lazy.sets.size() + 1);
			lazy.accept_begin.reserve(lazy.accept_begin.size() + 1);
			lazy.accepted.reserve(lazy.accepted.size() + ids.size());
			const uint32_t id = static_cast<uint32_t>(lazy.sets.size());
			const auto inserted = lazy.index.emplace(std::move(set), id).first;
			lazy.sets.push_back(&inserted->first);
			lazy.next.resize(lazy.next.size() + lazy.map.class_count, unknown);
			lazy.accepted.insert(lazy.accepted.end(), ids.begin(), ids.end());
			lazy.accept_begin.push_back(static_cast<uint32_t>(lazy.accepted.size()));
			lazy.bytes += added;
			lazy.next[slot] = id;
			return id;
		} catch (const std::bad_alloc&) {
			return unknown;  // this lookup simulates; the table keeps its earlier states
		}
	}

	// Walks the lazy table and copies the accepted ids to `ids`. False when the table is full on this
	// text's path: the caller simulates.
	template <typename RegexData, typename char_t>
	template <typename ConstIterator>
	bool RegexMatcher<RegexData, char_t>::lazy_ids(detail::LazyTable<RegexData>& lazy, ConstIterator first,
	                                               ConstIterator last, std::vector<RegexData>& ids) const {
		constexpr uint32_t unknown = detail::LazyTable<RegexData>::unknown;
		std::lock_guard<std::mutex> lock(lazy.mutex);
		uint32_t state = lazy.start;
		for (auto it = first; it != last && state != 0; ++it) {
			const uint32_t k = lazy.map.byte_class[static_cast<unsigned char>(static_cast<char_t>(*it))];
			uint32_t target = lazy.next[size_t(state) * lazy.map.class_count + k];
			if (target == unknown) {
				target = lazy_add(lazy, state, k);
				if (target == unknown) {
					return false;
				}
			}
			state = target;
		}
		ids.assign(lazy.accepted.begin() + lazy.accept_begin[state], lazy.accepted.begin() + lazy.accept_begin[state + 1]);
		return true;
	}

	// The slow path: builds the table once, under the mutex. Later callers find it ready.
	template <typename RegexData, typename char_t>
	const detail::LookupTable<RegexData>* RegexMatcher<RegexData, char_t>::build_table() const {
		std::lock_guard<std::mutex> lock(table_cache.mutex);
		if (const auto* ready = table_cache.ready.load(std::memory_order_acquire)) {
			return ready;
		}
		const auto started = std::chrono::steady_clock::now();
		auto built = std::make_unique<detail::LookupTable<RegexData>>();
		if constexpr (sizeof(char_t) == 1) {
			if (limits.max_states > 0) {
				try {
					const detail::ByteMap map = byte_map();
					const char* stopped = "off";
					if (limits.build_ahead) {
						stopped = fill_table(map, *built);
					}
					if (stopped != nullptr) {
						const size_t reached = built->stats.full_states_reached;
						built = std::make_unique<detail::LookupTable<RegexData>>();
						built->stats.full_stopped_by = stopped;
						built->stats.full_states_reached = reached;
						start_lazy(map, *built);
					}
				} catch (const std::bad_alloc&) {
					built = std::make_unique<detail::LookupTable<RegexData>>();  // simulate instead
					built->stats.full_stopped_by = "memory";
				}
			}
		}
		table_cache.builds++;
		built->stats.mode = built->mode;
		built->stats.builds = table_cache.builds;
		built->stats.build_time =
		    std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started);
		table_cache.owned = std::move(built);
		table_cache.ready.store(table_cache.owned.get(), std::memory_order_release);
		return table_cache.owned.get();
	}

	// The fast path: one acquire load and one branch while the table is built
	template <typename RegexData, typename char_t>
	const detail::LookupTable<RegexData>* RegexMatcher<RegexData, char_t>::table() const {
		if constexpr (sizeof(char_t) != 1) {
			return nullptr;
		} else {
			const auto* t = table_cache.ready.load(std::memory_order_acquire);
			if (t == nullptr) {
				t = build_table();
			}
			return t;
		}
	}

	namespace detail {
		// One table load per byte; stops at the dead state, row 0.
		template <typename char_t, typename Entry, typename ConstIterator>
		inline uint32_t walk_table(const Entry* table, const uint8_t* byte_class, uint32_t row, ConstIterator it,
		                           ConstIterator end) {
			for (; it != end; ++it) {
				row = table[row + byte_class[static_cast<unsigned char>(static_cast<char_t>(*it))]];
				if (row == 0) {
					break;
				}
			}
			return row;
		}
	}  // namespace detail

	// The state of a full table the text ends in
	template <typename RegexData, typename char_t>
	template <typename Iterable>
	uint32_t RegexMatcher<RegexData, char_t>::table_state(const detail::LookupTable<RegexData>& t,
	                                                      const Iterable& text) const {
		const auto walk = [&](auto first, auto last) {
			if (!t.narrow.empty()) {
				return detail::walk_table<char_t>(t.narrow.data(), t.byte_class.data(), t.start, first, last);
			}
			return detail::walk_table<char_t>(t.wide.data(), t.byte_class.data(), t.start, first, last);
		};
		uint32_t row = 0;
		if constexpr (detail::is_contiguous_text<Iterable>::value) {
			const auto* first = std::data(text);
			row = walk(first, first + std::size(text));
		} else {
			row = walk(std::cbegin(text), std::cend(text));
		}
		return row >> t.shift;
	}

	// The matching ids by simulating the automaton, sorted, each once
	template <typename RegexData, typename char_t>
	template <typename Iterable>
	void RegexMatcher<RegexData, char_t>::simulate_ids(detail::RunScratch<RegexData>& scratch,
	                                                   const Iterable& text) const {
		auto& ids = scratch.ids;
		ids.clear();
		run<false>(scratch, std::cbegin(text), std::cend(text), [&ids](const RegexData& id, const size_t*) {
			ids.push_back(id);
		});
		std::sort(ids.begin(), ids.end());
		ids.erase(std::unique(ids.begin(), ids.end(),
		                      [](const RegexData& a, const RegexData& b) { return !(a < b) && !(b < a); }),
		          ids.end());
	}

	// Calls visit(first, last) with the matching ids, sorted, each once, and returns what it returns.
	template <typename RegexData, typename char_t>
	template <typename Iterable, typename Visit>
	decltype(auto) RegexMatcher<RegexData, char_t>::with_ids(const Iterable& text, Visit&& visit) const {
		static_assert(!std::is_array_v<Iterable>,
		              "Pass a std::string or std::string_view, not a C array: its final '\\0' would be matched.");
		const auto* t = table();
		if (t != nullptr && t->mode == TableMode::full) {
			const uint32_t s = table_state(*t, text);
			const auto first = t->accepted.cbegin();
			return visit(first + t->accept_begin[s], first + t->accept_begin[s + 1]);
		}
		// The ids go to a scratch buffer lent for this call, so a callback may look up again: it gets
		// another buffer. The lease clears the ids when the call ends.
		detail::ScratchLease<RegexData> lease(scratch_pool);
		auto& scratch = lease.get();
		auto& ids = scratch.ids;
		if (t == nullptr || t->mode != TableMode::lazy || !lazy_ids(*t->lazy, std::cbegin(text), std::cend(text), ids)) {
			if (t != nullptr && t->mode == TableMode::lazy) {
				t->lazy->simulated_texts.fetch_add(1, std::memory_order_relaxed);
			}
			simulate_ids(scratch, text);
		}
		const IdIterator first = ids.cbegin();
		const IdIterator last = ids.cend();
		return visit(first, last);
	}

	template <typename RegexData, typename char_t>
	template <typename Iterable>
	std::vector<RegexData> RegexMatcher<RegexData, char_t>::match(const Iterable& str) const {
		return with_ids(str, [](IdIterator first, IdIterator last) {
			return first == last ? std::vector<RegexData>() : std::vector<RegexData>(first, last);
		});
	}

	template <typename RegexData, typename char_t>
	template <typename Iterable, typename Callback>
	size_t RegexMatcher<RegexData, char_t>::for_each_match(const Iterable& text, Callback&& callback) const {
		return with_ids(text, [&callback](IdIterator first, IdIterator last) {
			size_t calls = 0;
			for (; first != last; ++first) {
				calls++;
				if constexpr (std::is_same_v<std::invoke_result_t<Callback&, const RegexData&>, bool>) {
					if (!callback(static_cast<const RegexData&>(*first))) {
						break;
					}
				} else {
					callback(static_cast<const RegexData&>(*first));
				}
			}
			return calls;
		});
	}

	template <typename RegexData, typename char_t>
	template <typename Iterable, typename Container>
	size_t RegexMatcher<RegexData, char_t>::match_into(const Iterable& text, Container& out) const {
		return with_ids(text, [&out](IdIterator first, IdIterator last) {
			out.clear();
			out.insert(out.end(), first, last);
			return static_cast<size_t>(last - first);
		});
	}

	template <typename RegexData, typename char_t>
	template <typename Iterable>
	bool RegexMatcher<RegexData, char_t>::matches_any(const Iterable& text) const {
		return with_ids(text, [](IdIterator first, IdIterator last) { return first != last; });
	}

	template <typename RegexData, typename char_t>
	template <typename Iterable>
	std::vector<MatchResult<RegexData>> RegexMatcher<RegexData, char_t>::match_with_groups(const Iterable& str) const {
		static_assert(!std::is_array_v<Iterable>,
		              "Pass a std::string or std::string_view, not a C array: its final '\\0' would be matched.");
		std::vector<MatchResult<RegexData>> results;
		const auto* t = table();
		if (t != nullptr && t->mode == TableMode::full) {
			const uint32_t s = table_state(*t, str);
			if (t->accept_begin[s] == t->accept_begin[s + 1]) {
				return results;  // no regex matches
			}
		}
		detail::ScratchLease<RegexData> lease(scratch_pool);
		run<true>(lease.get(), std::cbegin(str), std::cend(str), [this, &results](const RegexData& id, const size_t* slots) {
			std::map<size_t, std::pair<size_t, size_t>> groups;
			for (size_t g = 0; g < max_groups; g++) {
				if (slots[2 * g] != detail::unset && slots[2 * g + 1] != detail::unset) {
					groups[g] = {slots[2 * g], slots[2 * g + 1]};
				}
			}
			results.emplace_back(id, std::move(groups));
		});
		// One result per regex id, the first one found (the highest priority), in ascending id order
		std::stable_sort(results.begin(), results.end(),
		                 [](const auto& a, const auto& b) { return a.regex_id < b.regex_id; });
		results.erase(std::unique(results.begin(), results.end(),
		                          [](const auto& a, const auto& b) {
			                          return !(a.regex_id < b.regex_id) && !(b.regex_id < a.regex_id);
		                          }),
		              results.end());
		return results;
	}

	template <typename RegexData, typename char_t>
	void RegexMatcher<RegexData, char_t>::compile() const {
		table();
	}

	template <typename RegexData, typename char_t>
	void RegexMatcher<RegexData, char_t>::set_table_limits(const TableLimits& new_limits) {
		limits = new_limits;
		table_cache.reset();
	}

	template <typename RegexData, typename char_t>
	TableStats RegexMatcher<RegexData, char_t>::table_stats() const {
		const auto* t = table_cache.ready.load(std::memory_order_acquire);
		if (t == nullptr) {
			t = build_table();
		}
		TableStats stats = t->stats;
		if (t->mode == TableMode::lazy) {
			const auto& lazy = *t->lazy;
			std::lock_guard<std::mutex> lock(lazy.mutex);
			stats.states = lazy.sets.size();
			stats.table_bytes = lazy.next.size() * sizeof(uint32_t);
			stats.total_bytes = lazy.bytes + lazy.accepted.size() * sizeof(RegexData);
			stats.simulated_texts = lazy.simulated_texts.load(std::memory_order_relaxed);
		}
		return stats;
	}

}  // namespace matcher
