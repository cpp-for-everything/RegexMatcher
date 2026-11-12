#pragma once

#include "RegexMatcherConfig.h"

#include <set>
#include <map>
#include <list>
#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <cstdint>
#include <memory>
#include <array>
#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <iterator>
#include <type_traits>
#include <utility>

#ifdef DEBUG
#include <iostream>
#endif

/**
 * @brief Public classes of the library
 *
 */
namespace matcher {
	/**
	 * @brief Contains the interface for adding regexes and matching a string with the trie
	 *
	 * @tparam RegexData Type of associated data with each regex
	 * @tparam char_t Type of the indiviidual symbols in the strings and regexes
	 */
	template <typename RegexData, typename char_t = char>
	class RegexMatcher;

	/**
	 * @brief Result of a regex match with captured groups
	 *
	 * @tparam RegexData Type of associated data with each regex
	 */
	template <typename RegexData>
	struct MatchResult {
		RegexData regex_id;
		std::map<size_t, std::pair<size_t, size_t>> groups;  // group_id -> (start_pos, end_pos)

		MatchResult(RegexData id, std::map<size_t, std::pair<size_t, size_t>> g) : regex_id(id), groups(std::move(g)) {}
	};

	/**
	 * @brief Default cap on the states of the lookup table (see RegexMatcher::set_table_limits)
	 *
	 */
	constexpr size_t default_table_max_states = size_t(1) << 16;

	/**
	 * @brief Default cap on the bytes the lookup table and its build may use
	 *
	 */
	constexpr size_t default_table_max_bytes = size_t(1) << 25;

	/**
	 * @brief Caps and build choice of the lookup table (see RegexMatcher::set_table_limits)
	 *
	 */
	struct TableLimits {
		size_t max_states = default_table_max_states;  // table states, the dead state included; 0 turns the table off
		size_t max_bytes = default_table_max_bytes;    // bytes of the table and of its build
		bool build_ahead = true;  // false: skip the full build and fill the table lazily, state by state
	};

	/**
	 * @brief How a matcher answers match() and its relatives
	 *
	 */
	enum class TableMode : uint8_t {
		full,       // a full table built ahead: one table load per byte
		lazy,       // a table filled during lookups, state by state, under a lock
		simulation  // no table: the automaton is simulated (wide char_t, or the table is off)
	};

	/**
	 * @brief What the last build of a matcher's lookup table produced (see RegexMatcher::table_stats)
	 *
	 */
	struct TableStats {
		TableMode mode = TableMode::simulation;
		size_t states = 0;        // table states now, the dead state included
		size_t byte_classes = 0;  // groups of byte values that every pattern treats alike
		size_t table_bytes = 0;   // bytes of the transitions
		size_t total_bytes = 0;   // bytes the table holds: transitions, byte map, accepted ids, state sets
		size_t builds = 0;        // tables this matcher object has built so far, the last one included
		std::chrono::nanoseconds build_time{0};  // time of the last build
		const char* full_stopped_by = nullptr;   // why the full build stopped: "states", "bytes", "work",
		                                         // "memory", "off" (build_ahead false); null when it finished
		size_t full_states_reached = 0;          // states the full build had when it stopped
		size_t simulated_texts = 0;  // lazy table: lookups that found it full and simulated the automaton
	};
}  // namespace matcher

/**
 * @brief Private helper classes of the library
 *
 * The matcher compiles every regex into one automaton with empty moves (a Thompson automaton).
 * Repeat counts are written out as copies of the repeated part, so the automaton has no counters.
 * Regexes share the part of the automaton that their identical leading items build (see
 * RegexMatcher::add_regex). Matching keeps the set of live states and steps it one character at a
 * time, so it never backtracks. For lookups without captures and an 8 bit char_t, a deterministic
 * table built from the automaton replaces that simulation (see impl/table.ipp).
 */
namespace matcher::detail {
	/**
	 * @brief The characters one step of the automaton accepts: '.', a single character or a set
	 *
	 */
	template <typename char_t>
	struct CharClass {
		bool any = false;                                // '.': every character but a line terminator
		std::vector<std::pair<char_t, char_t>> ranges;  // inclusive ranges, a single character is (c, c)

		bool contains(char_t c) const {
			if (any) {
				if (c == char_t('\n') || c == char_t('\r')) {
					return false;
				}
				if constexpr (sizeof(char_t) > 1) {
					if (c == char_t(0x2028) || c == char_t(0x2029)) {
						return false;
					}
				}
				return true;
			}
			for (const auto& [low, high] : ranges) {
				if (!(c < low) && !(high < c)) {
					return true;
				}
			}
			return false;
		}

		bool operator==(const CharClass& other) const { return any == other.any && ranges == other.ranges; }
	};

	/**
	 * @brief A parsed regex
	 *
	 */
	struct Ast {
		enum class Kind : uint8_t {
			Class,   // one character from classes[value]
			Group,   // capture group number `value` around children[0]
			Concat,  // children in sequence
			Alt,     // one of the children, the first has priority
			Repeat   // children[0] repeated min to max times
		};
		Kind kind = Kind::Concat;
		size_t value = 0;
		size_t min = 0;
		std::optional<size_t> max;
		bool lazy = false;
		std::vector<Ast> children;
	};

	/**
	 * @brief One state of the automaton
	 *
	 */
	struct State {
		enum class Kind : uint8_t {
			Char,    // consumes one character of classes[value], then goes to out[0]
			Split,   // empty moves to every state of out, the first has priority
			Open,    // records the start of group `value`, then goes to out[0]
			Close,   // records the end of group `value`, then goes to out[0]
			Reset,   // clears the captures of groups [value, value_end) for a new loop pass, then out[0]
			Accept,  // the regex accept_ids[value] matches when the text ends here
			Enter,   // starts an optional pass of a repeat at nesting level `value`, then out[0]
			Check    // ends that pass; for captures only, the pass fails when it consumed nothing
		};
		Kind kind = Kind::Split;
		uint32_t value = 0;
		uint32_t value_end = 0;
		std::vector<uint32_t> out;
	};

	/**
	 * @brief A node of the prefix tree over the leading items of the added regexes
	 *
	 * Two regexes whose first k items are the same (same parse tree, same group numbers) share the states
	 * of those items and the exit state at the end of them. Each regex continues from that exit state
	 * with states of its own, so sharing never changes what any regex matches.
	 */
	struct PrefixNode {
		uint32_t exit = 0;                                    // state reached after the items of this prefix
		std::vector<std::pair<std::string, size_t>> children;  // item key -> index of the longer prefix
	};

	/**
	 * @brief Capture slot value of a group that has not matched
	 *
	 */
	constexpr size_t unset = static_cast<size_t>(-1);

	/**
	 * @brief The deterministic lookup table built from the automaton by subset construction
	 *
	 * A table state is a set of automaton states. The 256 byte values are grouped into classes that every
	 * pattern treats alike. The transitions are one array: the entry of state s and class k is at
	 * (s << shift) + k, and it holds the target already shifted, so one step is
	 * `row = table[row + byte_class[byte]]`. Row 0 is the dead state: no pattern can match from it.
	 * The ids a state accepts are sorted, each once, and stored one state after the other.
	 */
	struct ByteMap {
		std::array<uint8_t, 256> byte_class{};
		size_t class_count = 1;
		uint32_t shift = 0;                         // log2 of the row width, the smallest power of 2 >= class_count
		std::vector<std::vector<uint16_t>> accepts;  // the byte classes of each character class of the automaton
	};

	// Memory of the closure over empty moves
	struct ClosureScratch {
		std::vector<uint64_t> mark;
		uint64_t stamp = 0;
		std::vector<uint32_t> stack;
	};

	struct StateSetHash {
		size_t operator()(const std::vector<uint32_t>& set) const noexcept {
			uint64_t hash = 1469598103934665603ull;  // FNV-1a
			for (const uint32_t s : set) {
				hash = (hash ^ s) * 1099511628211ull;
			}
			return static_cast<size_t>(hash);
		}
	};

	/**
	 * @brief The table filled during lookups (TableMode::lazy)
	 *
	 * Used when the full build passes its caps or its work bound, or when the limits ask for it. A lookup
	 * walks it under its mutex and computes a move that is still `unknown` on the way. When a new state
	 * would pass the caps, the table stays as it is and that lookup simulates the automaton from the
	 * first character.
	 *
	 * The mutex is a plain one, for reads too, so lookups on one lazy table do not run in parallel. (On
	 * MinGW-Builds g++ 15.2.0, x86_64-posix-seh, std::shared_mutex is a winpthreads rwlock whose
	 * lock_shared() failed under this use.)
	 */
	template <typename RegexData>
	struct LazyTable {
		static constexpr uint32_t unknown = 0xFFFFFFFFu;

		ByteMap map;
		size_t max_states = 0;
		size_t max_bytes = 0;
		uint32_t start = 0;

		mutable std::mutex mutex;
		std::vector<uint32_t> next;  // state * class_count + class -> target state, or unknown
		std::vector<const std::vector<uint32_t>*> sets;  // the set of each state, keys of index
		std::unordered_map<std::vector<uint32_t>, uint32_t, StateSetHash> index;
		std::vector<uint32_t> accept_begin;  // as in LookupTable
		std::vector<RegexData> accepted;
		size_t bytes = 0;
		ClosureScratch closure;
		std::vector<uint32_t> seeds;
		std::atomic<size_t> simulated_texts{0};
	};

	template <typename RegexData>
	struct LookupTable {
		TableMode mode = TableMode::simulation;
		uint32_t shift = 0;  // log2 of the row width
		uint32_t start = 0;  // row of the start state
		std::array<uint8_t, 256> byte_class{};
		std::vector<uint16_t> narrow;  // the transitions when every row offset fits 16 bits
		std::vector<uint32_t> wide;    // otherwise
		std::vector<uint32_t> accept_begin;  // accepted ids of state s: accepted[accept_begin[s], accept_begin[s+1])
		std::vector<RegexData> accepted;
		std::unique_ptr<LazyTable<RegexData>> lazy;  // TableMode::lazy only
		TableStats stats;
	};

	/**
	 * @brief The lazily built table of one matcher
	 *
	 * `ready` is null until a lookup or compile() builds the table under `mutex`. A copy starts empty and
	 * builds its own table. A move takes the table along.
	 */
	template <typename RegexData>
	struct TableCache {
		std::mutex mutex;
		std::atomic<const LookupTable<RegexData>*> ready{nullptr};
		std::unique_ptr<LookupTable<RegexData>> owned;
		size_t builds = 0;  // guarded by mutex

		TableCache() = default;
		TableCache(const TableCache&) noexcept {}
		TableCache(TableCache&& other) noexcept : owned(std::move(other.owned)), builds(other.builds) {
			ready.store(owned.get(), std::memory_order_release);
			other.ready.store(nullptr, std::memory_order_release);
		}
		TableCache& operator=(const TableCache& other) noexcept {
			if (this != &other) {
				reset();
			}
			return *this;
		}
		TableCache& operator=(TableCache&& other) noexcept {
			if (this != &other) {
				owned = std::move(other.owned);
				builds = other.builds;
				ready.store(owned.get(), std::memory_order_release);
				other.ready.store(nullptr, std::memory_order_release);
			}
			return *this;
		}
		~TableCache() = default;

		// Drops the table. Not safe while another thread looks up, like add_regex.
		void reset() noexcept {
			ready.store(nullptr, std::memory_order_release);
			owned.reset();
		}
	};

	/**
	 * @brief Working memory of one simulation run
	 *
	 * The matcher keeps them in a ScratchPool and reuses them, so a run does not allocate once the
	 * buffers have reached the size of the automaton. `mark` holds stamps that only grow, so values left by an earlier
	 * call or another matcher never look current.
	 */
	enum class RunAction : uint8_t {
		visit,    // visit `state`
		restore,  // put `value` back into work[slot]
		finish    // the visit seen[state][slot] has followed all its moves
	};
	struct RunItem {
		uint32_t state;
		RunAction action;
		size_t slot;
		size_t value;
		uint64_t open;  // bit L: an optional pass at level L started at this step (see State::Kind::Check)
	};
	struct RunSeen {
		uint64_t open;
		bool finished;
	};
	template <typename RegexData>
	struct RunScratch {
		std::vector<uint64_t> mark;  // mark[s] is the stamp of the last step whose list state s entered
		uint64_t stamp = 0;
		std::vector<uint32_t> current;
		std::vector<uint32_t> next;
		std::vector<size_t> current_slots;
		std::vector<size_t> next_slots;
		std::vector<size_t> work;
		std::vector<RunItem> stack;
		std::vector<std::vector<RunSeen>> seen;
		std::vector<RegexData> ids;  // the matching ids of a lookup
	};

	/**
	 * @brief Spare scratch buffers of one matcher, kept for its later lookups
	 *
	 * A lookup takes one with an atomic exchange, or allocates one when every slot is empty (more
	 * lookups at once than slots, or a callback that looks up again), and puts it back with a compare
	 * and exchange, or frees it when every slot is full. No thread or other matcher shares them. (Not
	 * thread_local: with MinGW-Builds g++ 15.2.0, x86_64-posix-seh, a thread_local object with a
	 * destructor corrupted the heap when its thread ended.) A copy starts empty; a move takes the
	 * buffers along.
	 */
	template <typename RegexData>
	struct ScratchPool {
		static constexpr size_t slot_count = 8;
		std::array<std::atomic<RunScratch<RegexData>*>, slot_count> slots{};

		ScratchPool() noexcept {
			for (auto& slot : slots) {
				slot.store(nullptr, std::memory_order_relaxed);
			}
		}
		ScratchPool(const ScratchPool&) noexcept : ScratchPool() {}
		ScratchPool(ScratchPool&& other) noexcept : ScratchPool() { take(other); }
		ScratchPool& operator=(const ScratchPool& other) noexcept {
			if (this != &other) {
				clear();
			}
			return *this;
		}
		ScratchPool& operator=(ScratchPool&& other) noexcept {
			if (this != &other) {
				clear();
				take(other);
			}
			return *this;
		}
		~ScratchPool() { clear(); }

		RunScratch<RegexData>* acquire() {
			for (auto& slot : slots) {
				if (auto* scratch = slot.exchange(nullptr, std::memory_order_acquire)) {
					return scratch;
				}
			}
			return new RunScratch<RegexData>();
		}
		void release(RunScratch<RegexData>* scratch) noexcept {
			for (auto& slot : slots) {
				RunScratch<RegexData>* empty = nullptr;
				if (slot.compare_exchange_strong(empty, scratch, std::memory_order_release, std::memory_order_relaxed)) {
					return;
				}
			}
			delete scratch;
		}
		// Frees the buffers. Not safe while another thread looks up, like add_regex.
		void clear() noexcept {
			for (auto& slot : slots) {
				delete slot.exchange(nullptr, std::memory_order_acq_rel);
			}
		}

	private:
		void take(ScratchPool& other) noexcept {
			for (size_t i = 0; i < slot_count; i++) {
				slots[i].store(other.slots[i].exchange(nullptr, std::memory_order_acq_rel), std::memory_order_release);
			}
		}
	};

	/**
	 * @brief Lends a scratch buffer of the pool for one lookup
	 *
	 */
	template <typename RegexData>
	class ScratchLease {
	public:
		explicit ScratchLease(ScratchPool<RegexData>& pool) : pool_(pool), scratch_(pool.acquire()) {}
		~ScratchLease() {
			scratch_->ids.clear();  // ids of the caller's type do not outlive the lookup
			pool_.release(scratch_);
		}
		ScratchLease(const ScratchLease&) = delete;
		ScratchLease& operator=(const ScratchLease&) = delete;

		RunScratch<RegexData>& get() { return *scratch_; }

	private:
		ScratchPool<RegexData>& pool_;
		RunScratch<RegexData>* scratch_;
	};

	// True when the text has std::data and std::size, so the table walk can use a pointer.
	template <typename T, typename = void>
	struct is_contiguous_text : std::false_type {};
	template <typename T>
	struct is_contiguous_text<T, std::void_t<decltype(std::data(std::declval<const T&>())),
	                                         decltype(std::size(std::declval<const T&>()))>> : std::true_type {};
}  // namespace matcher::detail

namespace matcher {
	template <typename RegexData, typename char_t>
	class RegexMatcher {
		std::vector<detail::State> states;  // states[0] is the start state
		std::vector<detail::CharClass<char_t>> classes;
		std::vector<RegexData> accept_ids;  // regex of each accept state, by accept index
		std::vector<detail::PrefixNode> prefixes;
		size_t max_groups = 0;  // the most groups any added regex has
		bool has_checks = false;  // some state is a Check
		uint32_t check_level = 0;  // nesting level of checked passes, used while add_regex builds
		TableLimits limits;
		mutable detail::TableCache<RegexData> table_cache;
		mutable detail::ScratchPool<RegexData> scratch_pool;

		using IdIterator = typename std::vector<RegexData>::const_iterator;

		template <typename ConstIterator>
		struct Parser;

		size_t add_class(const detail::CharClass<char_t>& cls);
		uint32_t new_state(detail::State::Kind kind, uint32_t value = 0);
		uint32_t build(const detail::Ast& ast, uint32_t next);
		uint32_t build_repeat(const detail::Ast& ast, uint32_t next);
		void add_sequence(const std::vector<detail::Ast>& items, uint32_t accept);

		template <bool WithGroups, typename ConstIterator, typename Sink>
		void run(detail::RunScratch<RegexData>& scratch, ConstIterator begin, ConstIterator end, Sink&& sink) const;

		const detail::LookupTable<RegexData>* table() const;
		const detail::LookupTable<RegexData>* build_table() const;
		detail::ByteMap byte_map() const;
		void closure(const std::vector<uint32_t>& seeds, std::vector<uint32_t>& set, detail::ClosureScratch& scratch) const;
		void seeds_for(const std::vector<uint32_t>& set, const detail::ByteMap& map, size_t byte_class,
		               std::vector<uint32_t>& seeds) const;
		void accepted_ids(const std::vector<uint32_t>& set, std::vector<RegexData>& out) const;
		const char* fill_table(const detail::ByteMap& map, detail::LookupTable<RegexData>& out) const;
		bool start_lazy(const detail::ByteMap& map, detail::LookupTable<RegexData>& out) const;
		uint32_t lazy_add(detail::LazyTable<RegexData>& lazy, uint32_t state, uint32_t byte_class) const;
		template <typename ConstIterator>
		bool lazy_ids(detail::LazyTable<RegexData>& lazy, ConstIterator first, ConstIterator last,
		              std::vector<RegexData>& ids) const;
		template <typename Iterable>
		uint32_t table_state(const detail::LookupTable<RegexData>& t, const Iterable& text) const;
		template <typename Iterable>
		void simulate_ids(detail::RunScratch<RegexData>& scratch, const Iterable& text) const;
		template <typename Iterable, typename Visit>
		decltype(auto) with_ids(const Iterable& text, Visit&& visit) const;

	public:
		/**
		 * @brief Construct a new Regex Matcher object
		 *
		 */
		RegexMatcher();

		/**
		 * @brief Adds another regex to the set of regexes
		 *
		 * @throws std::invalid_argument for a malformed pattern, for syntax that is not supported (anchors
		 * '^' and '$', negated sets, escapes of letters and digits such as \d, a repeat right after a
		 * repeat such as a**), or when the repeat counts write out more states than one pattern may have.
		 * The matcher is unchanged when it throws.
		 *
		 * @tparam Iterable Set of characters for the regex. Must implement std::cbegin and std::cend
		 * @tparam RegexData Value that will be associated with the regex
		 */
		template <typename Iterable>
		void add_regex(Iterable, RegexData);

		/**
		 * @brief Matches a string with all added regexes
		 *
		 * @note Safe to call from several threads at once on one matcher, as long as no thread calls
		 * add_regex() or set_table_limits() at the same time. The first lookup after a change builds the
		 * lookup table (see compile()). It allocates only the returned vector, and nothing when no regex
		 * matches.
		 *
		 * @tparam Iterable Set of characters. Must implement std::cbegin and std::cend. Not a C array:
		 * pass a std::string or std::string_view, so a string literal's final '\0' is not matched.
		 * @return std::vector<RegexData> Set of regexes' UIDs that match the text, each once, in ascending order
		 */
		template <typename Iterable>
		std::vector<RegexData> match(const Iterable&) const;

		/**
		 * @brief Calls `callback(id)` for each regex that matches the text, in the order of match()
		 *
		 * It allocates nothing when the lookup table is in use. When the callback returns bool, false stops
		 * the calls. Same thread rules as match().
		 *
		 * @return size_t How many times the callback was called
		 */
		template <typename Iterable, typename Callback>
		size_t for_each_match(const Iterable& text, Callback&& callback) const;

		/**
		 * @brief Replaces the contents of `out` with the ids match() returns
		 *
		 * `out` is a container with clear() and insert(end, first, last), such as std::vector. Reusing it
		 * avoids the allocation of match(). Same thread rules as match().
		 *
		 * @return size_t The number of ids
		 */
		template <typename Iterable, typename Container>
		size_t match_into(const Iterable& text, Container& out) const;

		/**
		 * @brief True when at least one regex matches the text. Same thread rules as match().
		 *
		 */
		template <typename Iterable>
		bool matches_any(const Iterable& text) const;

		/**
		 * @brief Matches a string with all added regexes and returns captured groups
		 *
		 * @note Same as match(): safe for concurrent calls while no thread changes the matcher. It returns the
		 * same regexes as match(), each once, in ascending order. The captures follow ECMAScript: those of
		 * the match a backtracking engine finds first, a new pass of a repeat forgets the captures of the
		 * last pass, and an optional pass that consumes nothing does not count. When the lookup table says
		 * that no regex matches, it returns at once.
		 *
		 * @tparam Iterable Set of characters. Must implement std::cbegin and std::cend. Not a C array.
		 * @return std::vector<MatchResult<RegexData>> Set of matches with captured group positions
		 */
		template <typename Iterable>
		std::vector<MatchResult<RegexData>> match_with_groups(const Iterable&) const;

		/**
		 * @brief Builds the lookup table now, so the first lookup is not slow
		 *
		 * Lookups build it on first use anyway. Safe to call from several threads, and at the same time as
		 * lookups; the table is built once. For 8 bit char_t only: with a wider char_t it does nothing and
		 * lookups simulate the automaton.
		 */
		void compile() const;

		/**
		 * @brief Caps the lookup table and chooses how it is built
		 *
		 * The full build stops when it would pass max_states or max_bytes, or when its work passes a bound
		 * proportional to the size of the automaton. The matcher then fills a lazy table during lookups,
		 * under the same caps; a lookup that would pass them simulates the automaton. Every mode gives the
		 * same answers. max_states 0 turns the table off. Drops the current table. Not safe while another
		 * thread uses the matcher, like add_regex().
		 */
		void set_table_limits(const TableLimits& new_limits);

		/**
		 * @brief The limits set by set_table_limits(), or the defaults
		 *
		 */
		const TableLimits& table_limits() const { return limits; }

		/**
		 * @brief Builds the lookup table if needed and describes it
		 *
		 */
		TableStats table_stats() const;

#ifdef DEBUG
		/**
		 * @brief Prints the states of the automaton
		 *
		 */
		void print() const;
#endif
	};
}  // namespace matcher

#include <matcher/impl/core.ipp>
#include <matcher/impl/node.ipp>
#include <matcher/impl/table.ipp>
