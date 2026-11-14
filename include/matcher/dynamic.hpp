#pragma once

/**
 * @file dynamic.hpp
 * @brief matcher::DynamicSet: the minimal multi-pattern automaton D_P, kept exact under add and remove
 *
 * A pattern set P maps ids to regular languages over the 256 byte values. For each pattern, A_r is its
 * minimal complete DFA. D_P is the reachable product of all A_r. A state of D_P is identified by its key:
 * the list of (pattern, component) pairs for the components that are not in their pattern's dead state.
 * The output of a state is the set of ids whose component accepts. D_P is the minimal Moore machine of
 * the match function, so the structure is history independent up to isomorphism.
 *
 * Updates follow the algorithms ADD, GARBAGE, REMOVE and REFINE of the theory notes of paper p10
 * (theory/proofs.tex): ADD explores only the live pairs, never writes an old row (outside REFINE),
 * and finds the old states that became unreachable by a least fixpoint over the candidate set, with
 * in-degree counters that include a virtual initial edge. REMOVE projects every live state onto the
 * state for its projected key, merging into an existing state or creating one. Classes are never
 * merged.
 *
 * Three table layouts, chosen at construction (DynamicLayout):
 *   slot_tagged   per-slot class maps (16 slots by default) with slot recycling; one dependent memory
 *                 read per input byte (design report A7)
 *   dense_spare   one global class map, rows with spare capacity (design report A2)
 *   full_bytes    256 columns, no classes (design report A5)
 *
 * Thread rules: lookups are read only and may run in parallel on a quiescent structure. An update must
 * not run at the same time as any lookup or other update. Concurrent lookup during an update is NOT
 * supported.
 *
 * Header only, C++17. It includes core.hpp, which is not changed by it (core.hpp only declares
 * detail::RegexAutomatonAccess as a friend of RegexMatcher, so that compile_regex_dfa can read the
 * automaton of the library's parser).
 */

#include <matcher/core.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace matcher {

	/**
	 * @brief A complete DFA over the 256 byte values, with its own byte classes
	 *
	 * delta[s * class_count + byte_class[b]] is the target of state s on byte b. dead is the state from
	 * which no accepting state is reachable, or none.
	 */
	struct MinimalDfa {
		static constexpr uint32_t none = 0xFFFFFFFFu;
		uint32_t state_count = 0;
		uint32_t class_count = 0;
		std::array<uint16_t, 256> byte_class{};
		std::vector<uint32_t> delta;
		std::vector<uint8_t> accepting;
		uint32_t initial = 0;
		uint32_t dead = none;

		uint32_t next(uint32_t state, uint8_t byte) const {
			return delta[size_t(state) * class_count + byte_class[byte]];
		}
	};

	/**
	 * @brief Throws std::invalid_argument when the DFA is not a complete DFA over 256 bytes
	 *
	 */
	inline void validate_dfa(const MinimalDfa& dfa);

	/**
	 * @brief The minimal DFA of the same language: accessible part, Moore minimization, states renumbered
	 * in breadth first order from the initial state, dead state identified. Keeps the byte classes as given
	 * (it does not coarsen them).
	 *
	 */
	inline MinimalDfa minimize_dfa(const MinimalDfa& dfa);

	/**
	 * @brief Compiles a regex with the parser of RegexMatcher (same syntax and same meaning: '.' is every
	 * byte but '\n' and '\r'), then determinizes, completes and minimizes it
	 *
	 * @throws std::invalid_argument as RegexMatcher::add_regex, std::length_error when the DFA passes max_states
	 */
	template <typename Iterable>
	MinimalDfa compile_regex_dfa(const Iterable& pattern, size_t max_states = size_t(1) << 18);

	enum class DynamicLayout : uint8_t {
		slot_tagged,  // L1: per-slot class maps, slot recycling, one dependent read per byte
		dense_spare,  // L2: one global class map, rows with spare capacity
		full_bytes    // L3: 256 columns, no classes
	};

	inline const char* layout_name(DynamicLayout layout) {
		switch (layout) {
			case DynamicLayout::slot_tagged:
				return "L1_slot_tagged";
			case DynamicLayout::dense_spare:
				return "L2_dense_spare";
			default:
				return "L3_full_bytes";
		}
	}

	struct DynamicOptions {
		DynamicLayout layout = DynamicLayout::slot_tagged;
		unsigned slots = 16;                         // L1: number of slots, 1 to 16
		unsigned slot_reserve_log2 = 26;             // L1: address space reserved per slot, in 4 byte cells
		size_t max_states = size_t(1) << 22;         // an add that would pass it throws std::length_error
		bool minimize_input = true;                  // add_dfa minimizes its input first
	};

	/**
	 * @brief The instrumentation of one update
	 *
	 */
	struct UpdateStats {
		bool is_add = true;
		bool empty_language = false;        // add of an empty language: nothing changes (ADD step 1)
		size_t live_region = 0;             // |Live_P'(i)| on add, |Live_P(i)| on remove
		size_t created = 0;                 // states created
		size_t deleted = 0;                 // add: garbage; remove: live states deleted
		size_t merged = 0;                  // remove: live states projected onto a state that existed before
		size_t candidates = 0;              // add: |C|, old states that are first components of live pairs
		size_t garbage_row_reads = 0;       // add: rows read by GARBAGE (2 |C|)
		size_t old_rows_read = 0;           // rows of pre-existing states read by the exploration
		size_t old_rows_written = 0;        // rows of pre-existing states written outside REFINE: must be 0
		size_t cells_written = 0;           // cells of new rows plus cells written by REFINE
		size_t refine_rows_rewritten = 0;   // rows written by REFINE (L1: slot recycle; L2: new columns or move)
		size_t refine_cells_written = 0;    // cells written by REFINE
		size_t class_splits = 0;            // 1 when this update refined the class partition
		size_t slot_recycles = 0;           // L1: 1 when the split recycled a slot
		size_t states = 0;                  // states after the update
		size_t classes = 0;                 // classes after the update
		size_t slots_in_use = 0;            // L1: slots that hold states, or the current one
		size_t table_bytes = 0;             // bytes of the rows and class maps (memory() gives the rest)
	};

	struct DynamicMemory {
		size_t table_bytes = 0;       // rows (allocated, holes included) and class maps
		size_t table_live_bytes = 0;  // cells of live states only, and class maps
		size_t key_bytes = 0;         // keys of live components
		size_t index_bytes = 0;       // hash index from keys to states
		size_t live_set_bytes = 0;    // per-pattern sets of live states
		size_t counter_bytes = 0;     // in-degree counters
		size_t output_bytes = 0;      // output id lists
		size_t other_bytes = 0;       // per-state hashes, stamps, slot fields, free lists
		size_t pattern_bytes = 0;     // the stored A_r
		size_t aux_bytes() const {
			return key_bytes + index_bytes + live_set_bytes + counter_bytes + output_bytes + other_bytes;
		}
	};

	/**
	 * @brief The represented machine, renumbered densely: transitions on all 256 bytes and outputs
	 *
	 */
	template <typename Id>
	struct DynamicSnapshot {
		size_t states = 0;
		uint32_t initial = 0;
		std::vector<uint32_t> next;  // state * 256 + byte
		std::vector<std::vector<Id>> outputs;  // sorted ids
	};

	namespace detail {
		template <typename Id>
		class DynamicEngine;
	}

	/**
	 * @brief The pattern set with its minimal multi-pattern automaton, kept exact under add and remove
	 *
	 * @tparam Id Pattern id: copyable, with operator<
	 */
	template <typename Id>
	class DynamicSet {
	public:
		explicit DynamicSet(const DynamicOptions& options = DynamicOptions());
		DynamicSet(DynamicSet&&) noexcept = default;
		DynamicSet& operator=(DynamicSet&&) noexcept = default;
		DynamicSet(const DynamicSet&) = delete;
		DynamicSet& operator=(const DynamicSet&) = delete;
		~DynamicSet() = default;

		/**
		 * @brief Adds a pattern given as a regex (compile_regex_dfa)
		 *
		 * @throws std::invalid_argument for a bad pattern or an id already present, std::length_error when the
		 * automaton would pass max_states (the set is then unchanged)
		 */
		template <typename Iterable>
		UpdateStats add_regex(const Iterable& pattern, const Id& id);

		/**
		 * @brief Adds a pattern given as a complete DFA over 256 bytes (minimized first unless the options say no)
		 *
		 */
		UpdateStats add_dfa(const MinimalDfa& dfa, const Id& id);

		/**
		 * @brief Removes a pattern. Throws std::invalid_argument when the id is not present.
		 *
		 */
		UpdateStats remove(const Id& id);

		bool contains(const Id& id) const { return pid_of_.count(id) != 0; }
		size_t pattern_count() const { return pid_of_.size(); }

		/**
		 * @brief The ids whose pattern matches the whole text, sorted, each once
		 *
		 * @tparam Iterable bytes with std::cbegin and std::cend (std::string, std::string_view, std::vector)
		 */
		template <typename Iterable>
		std::vector<Id> match(const Iterable& text) const;

		/**
		 * @brief Calls callback(id) for each matching id, in the order of match(). A callback that returns
		 * bool stops the calls with false. Returns the number of calls.
		 *
		 */
		template <typename Iterable, typename Callback>
		size_t for_each_match(const Iterable& text, Callback&& callback) const;

		/**
		 * @brief Replaces the contents of out with the ids match() returns. Returns their number.
		 *
		 */
		template <typename Iterable, typename Container>
		size_t match_into(const Iterable& text, Container& out) const;

		/**
		 * @brief True when at least one pattern matches the whole text
		 *
		 */
		template <typename Iterable>
		bool matches_any(const Iterable& text) const;

		size_t state_count() const;
		size_t class_count() const;
		DynamicLayout layout() const { return options_.layout; }
		const DynamicOptions& options() const { return options_; }
		DynamicMemory memory() const;

		/**
		 * @brief The represented machine, for tests and checks
		 *
		 */
		DynamicSnapshot<Id> snapshot() const;

		/**
		 * @brief Recomputes counters, index, live sets and reachability from scratch and compares them with
		 * the stored ones. With deep, also checks every transition of every state against the keys and the
		 * stored A_r. Returns an empty string when everything holds, else a description of the first problems.
		 *
		 */
		std::string check_invariants(bool deep = false) const;

		/**
		 * @brief The state the text ends in (an internal record number), for tests and benchmarks
		 *
		 */
		uint32_t final_state(const uint8_t* text, size_t length) const;

		/**
		 * @brief The ids of a state returned by final_state
		 *
		 */
		const std::vector<Id>& outputs_of(uint32_t state) const;

		/**
		 * @brief The live record numbers, and the represented transition of one record, for tests
		 *
		 */
		std::vector<uint32_t> records() const;
		uint32_t transition(uint32_t state, uint8_t byte) const;

	private:
		DynamicOptions options_;
		std::unique_ptr<detail::DynamicEngine<Id>> engine_;
		std::map<Id, uint32_t> pid_of_;  // id -> internal pattern number (never reused)
		uint32_t next_pid_ = 0;

		template <typename Iterable, typename Visit>
		decltype(auto) with_ids(const Iterable& text, Visit&& visit) const;
	};

}  // namespace matcher

#include <matcher/impl/dynamic.ipp>
