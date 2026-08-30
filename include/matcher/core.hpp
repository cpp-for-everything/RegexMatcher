#pragma once

#include "RegexMatcherConfig.h"

#include <algorithm>
#include <set>
#include <map>
#include <list>
#include <string>
#include <vector>
#include <optional>
#include <sstream>
#include <cstdint>
#include <memory>

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
}  // namespace matcher

/**
 * @brief Private helper functions and classes for the library
 *
 */
namespace {

	/**
	 * @brief Class containing the min and max repeat of the same edge
	 *
	 */
	struct Limits;

	/**
	 * @brief Tag action for tagged automaton transitions
	 * Represents capture operations: opening or closing a group at a position
	 */
	struct TagAction {
		enum class Type : uint8_t {
			OPEN_GROUP,  // Start capturing group_id
			CLOSE_GROUP  // End capturing group_id
		};

		Type type;
		size_t group_id;

		TagAction(Type t, size_t id) : type(t), group_id(id) {}

		static TagAction open(size_t group_id) { return TagAction(Type::OPEN_GROUP, group_id); }
		static TagAction close(size_t group_id) { return TagAction(Type::CLOSE_GROUP, group_id); }

		bool is_open() const { return type == Type::OPEN_GROUP; }
		bool is_close() const { return type == Type::CLOSE_GROUP; }
	};

	/**
	 * @brief Capture slots for efficient group position tracking
	 * Uses vector for O(1) access instead of map
	 * Auto-resizes when accessing groups beyond current capacity
	 * Supports undo operations for efficient backtracking without full copies
	 */
	struct CaptureSlots {
		static constexpr size_t UNSET = static_cast<size_t>(-1);

		std::vector<size_t> start_positions;  // group_id -> start position (UNSET if not started)
		std::vector<size_t> end_positions;    // group_id -> end position (UNSET if not ended)

		CaptureSlots() = default;

		void ensure_capacity(size_t group_id) {
			if (group_id >= start_positions.size()) {
				start_positions.resize(group_id + 1, UNSET);
				end_positions.resize(group_id + 1, UNSET);
			}
		}

		// Returns previous value for undo
		size_t open_group(size_t group_id, size_t position) {
			ensure_capacity(group_id);
			size_t prev = start_positions[group_id];
			start_positions[group_id] = position;
			return prev;
		}

		// Returns previous value for undo
		size_t close_group(size_t group_id, size_t position) {
			ensure_capacity(group_id);
			size_t prev = end_positions[group_id];
			end_positions[group_id] = position;
			return prev;
		}

		// Restore previous start value
		void undo_open(size_t group_id, size_t prev_value) { start_positions[group_id] = prev_value; }

		// Restore previous end value
		void undo_close(size_t group_id, size_t prev_value) { end_positions[group_id] = prev_value; }

		bool is_group_complete(size_t group_id) const {
			return group_id < start_positions.size() && start_positions[group_id] != UNSET &&
			       end_positions[group_id] != UNSET;
		}

		std::map<size_t, std::pair<size_t, size_t>> to_map() const {
			std::map<size_t, std::pair<size_t, size_t>> result;
			for (size_t i = 0; i < start_positions.size(); ++i) {
				if (start_positions[i] != UNSET && end_positions[i] != UNSET) {
					result[i] = {start_positions[i], end_positions[i]};
				}
			}
			return result;
		}
	};

	/**
	 * @brief One match's capture slots, per regex, in a short run
	 *
	 * Keyed by regex, and a route lookup only ever tags one or two of them, so a tree
	 * allocated a node per match to hold a couple of entries. Searched linearly, which
	 * at these sizes is both faster and free of allocation until something is actually
	 * captured.
	 */
	template <typename RegexData>
	struct CaptureTable {
		std::vector<std::pair<RegexData, CaptureSlots>> entries;

		CaptureSlots& operator[](const RegexData& id) {
			for (auto& entry : entries) {
				if (entry.first == id) {
					return entry.second;
				}
			}
			entries.emplace_back(id, CaptureSlots{});
			return entries.back().second;
		}
	};

	/**
	 * @brief An ordered map held in one contiguous run instead of a tree
	 *
	 * The edges leaving a node were a std::map, so every character of a lookup chased
	 * red-black pointers through separately allocated nodes. Nodes here have one or two
	 * edges almost everywhere, which is the case a tree is worst at: the allocation and
	 * the indirection cost more than the comparison saves.
	 *
	 * Sorted by key, searched by binary search, so ordered iteration still works and the
	 * calling code did not have to change. Only the operations the matcher actually uses
	 * are here; this is not a general container.
	 *
	 * Values move when the run grows, so nothing may hold a pointer into it across an
	 * insertion. The only thing that does is the class-edge list, which compile() builds
	 * after the graph is final and rebuilds whenever a regex is added.
	 */
	template <typename Key, typename Value>
	class flat_map {
		std::vector<std::pair<Key, Value>> entries;

		static bool by_key(const std::pair<Key, Value>& entry, const Key& key) { return entry.first < key; }

	public:
		using iterator = typename std::vector<std::pair<Key, Value>>::iterator;
		using const_iterator = typename std::vector<std::pair<Key, Value>>::const_iterator;

		iterator begin() { return entries.begin(); }
		iterator end() { return entries.end(); }
		const_iterator begin() const { return entries.begin(); }
		const_iterator end() const { return entries.end(); }
		const_iterator cbegin() const { return entries.cbegin(); }
		const_iterator cend() const { return entries.cend(); }

		size_t size() const { return entries.size(); }
		bool empty() const { return entries.empty(); }

		iterator find(const Key& key) {
			auto it = std::lower_bound(entries.begin(), entries.end(), key, by_key);
			return (it != entries.end() && !(key < it->first)) ? it : entries.end();
		}

		const_iterator find(const Key& key) const {
			auto it = std::lower_bound(entries.begin(), entries.end(), key, by_key);
			return (it != entries.end() && !(key < it->first)) ? it : entries.end();
		}

		Value& operator[](const Key& key) {
			auto it = std::lower_bound(entries.begin(), entries.end(), key, by_key);
			if (it != entries.end() && !(key < it->first)) {
				return it->second;
			}
			return entries.emplace(it, key, Value{})->second;
		}

		/// Moves a value in under a key that is not already present.
		void insert_moved(const Key& key, Value&& value) {
			auto it = std::lower_bound(entries.begin(), entries.end(), key, by_key);
			if (it != entries.end() && !(key < it->first)) {
				it->second = std::move(value);
				return;
			}
			entries.emplace(it, key, std::move(value));
		}

		iterator erase(iterator it) { return entries.erase(it); }

		/// Inserts if the key is absent, and reports whether it did.
		std::pair<iterator, bool> emplace(const Key& key, const Value& value) {
			auto it = std::lower_bound(entries.begin(), entries.end(), key, by_key);
			if (it != entries.end() && !(key < it->first)) {
				return {it, false};
			}
			return {entries.emplace(it, key, value), true};
		}

		Value& at(const Key& key) { return find(key)->second; }
		const Value& at(const Key& key) const { return find(key)->second; }

		/// Takes over every entry of `other` whose key is absent here, leaving the rest.
		/// std::map::merge, which this replaces, does the same.
		void merge(flat_map& other) {
			for (auto it = other.entries.begin(); it != other.entries.end();) {
				auto here = std::lower_bound(entries.begin(), entries.end(), it->first, by_key);
				if (here != entries.end() && !(it->first < here->first)) {
					++it;
					continue;
				}
				entries.emplace(here, it->first, std::move(it->second));
				it = other.entries.erase(it);
			}
		}

		void reserve(size_t n) { entries.reserve(n); }
	};

	/**
	 * @brief Class containing the list of regexes using the given edge
	 *
	 * @tparam T type of the reference to the match data
	 * @tparam char_t Type of symbols used, for the collapsed literal run
	 */
	template <typename T, typename Node, typename char_t>
	struct EdgeInfo;

	/**
	 * @brief Contains the characters and additional attributes for wildcard symbols and Null symbols
	 *
	 */
	template <typename char_t>
	struct symbol;

	/**
	 * @brief Node in the trie-grah
	 *
	 * @tparam RegexData Type of the data associated with each regex
	 * @tparam char_t Type of symbols used
	 */
	template <typename RegexData, typename char_t>
	class Node;

	template <typename Node_T>
	class SubTree;
}  // namespace

namespace {
	struct Limits {
		size_t min;
		std::optional<size_t> max;

		Limits() : Limits(0, std::nullopt) {}

		Limits(size_t min, std::nullopt_t) {
			this->min = min;
			this->max = std::nullopt;
		}

		Limits(std::nullopt_t, size_t max) : Limits(0, max) {}

		Limits(size_t min, size_t max) {
			this->min = min;
			this->max = max;
		}

		static const Limits common_edge;
		static const Limits zero_or_once;
		static const Limits once_or_more;
		static const Limits zero_or_more;

		static std::string to_string(std::optional<Limits*> it) {
			if (!it.has_value()) {
				return "";
			}
			std::stringstream ss;
			if (!it.value()->max.has_value()) {
				ss << "(" << it.value()->min << "+)";
			} else {
				ss << "(" << it.value()->min << "..." << it.value()->max.value() << ")";
			}
			return ss.str();
		}

		static std::string to_string(Limits a) {
			std::stringstream ss;
			if (!a.max.has_value()) {
				ss << "(" << a.min << "+)";
			} else {
				ss << "(" << a.min << "..." << a.max.value() << ")";
			}
			return ss.str();
		}

		Limits& operator--() {
			if (min > 0) {
				min--;
			}
			if (max.has_value() && max.value() > 0) {
				max = max.value() - 1;
			}
			return *this;
		}

		bool is_required() const { return min > 0; }

		bool is_allowed_to_repeat() const {
			return !max.has_value() || (max.value() > 0 && this->min <= this->max.value());
		}
	};

	const Limits Limits::common_edge = Limits();
	const Limits Limits::zero_or_once = Limits(0, 1);
	const Limits Limits::once_or_more = Limits(1, std::nullopt);
	const Limits Limits::zero_or_more = Limits(0, std::nullopt);

	/**
	 * @brief A match's private view of the repeat counters
	 *
	 * The Limits objects live in the matcher and are shared by every caller, so matching
	 * must not decrement them in place: two threads matching on one RegexMatcher would
	 * race on the same counter, and a counter read while another thread was restoring it
	 * could drop a match. Traversal keeps its own copy of each counter it touches here,
	 * alongside the CaptureSlots, and the graph stays read-only while matching.
	 *
	 * It did show up in the routing benchmark. A std::map here allocated a tree node the
	 * first time a match touched a repeat counter, and one route lookup touches one or
	 * two: the allocation cost more than everything it was tracking. A short run searched
	 * linearly is faster at these sizes and allocates once, if at all.
	 */
	struct LimitState {
		std::vector<std::pair<const Limits*, Limits>> counters;

		const Limits& current(const Limits* base) const {
			for (const auto& entry : counters) {
				if (entry.first == base) {
					return entry.second;
				}
			}
			return *base;
		}

		/// Decrements this match's copy and returns the value before it, for undo.
		Limits consume(const Limits* base) {
			for (auto& entry : counters) {
				if (entry.first == base) {
					const Limits old = entry.second;
					--(entry.second);
					return old;
				}
			}
			counters.emplace_back(base, *base);
			const Limits old = counters.back().second;
			--(counters.back().second);
			return old;
		}

		void restore(const Limits* base, const Limits& old) {
			for (auto& entry : counters) {
				if (entry.first == base) {
					entry.second = old;
					return;
				}
			}
			counters.emplace_back(base, old);
		}
	};

	template <typename T, typename Node, typename char_t>
	struct EdgeInfo {
		// Flat rather than trees. Along a shared prefix one edge carries every pattern in
		// the table, and at ten thousand routes a red-black node apiece was the largest
		// single thing in the structure; tag_actions is empty on almost every edge and
		// was paying for a tree header regardless.
		flat_map<T, std::optional<Limits*>>
		    paths;  // each path may have different requirements for how many times should the edge be repeated.
		flat_map<T, std::vector<TagAction>> tag_actions;  // tag actions per regex path for capture tracking
		Node* to;

		// --- filled by RegexMatcher::compile, read only while matching -------
		//
		// Matching used to rebuild the set of still-live regexes at every character by
		// intersecting the arriving set with this edge's. Along a shared prefix that
		// set is every regex in the table, so a lookup cost one pass over the whole
		// table per character: at a thousand routes under /api/v1/ it was measured at
		// nine thousand map-node visits for one match.
		//
		// None of that work depends on the input. Which regexes an edge keeps alive is
		// a property of the graph, so it is computed once here and the walk just reads
		// it.

		/// This edge's keys, sorted, in one cache-friendly run rather than a red-black
		/// tree. Handed straight to the next frame when the fast path applies, so a
		/// deterministic step allocates nothing.
		std::vector<T> ids;

		/// Whether any path on this edge carries a repeat limit. When none does, no
		/// path can be pruned here and the arriving set passes through untouched.
		bool has_limits = false;

		/// Whether `ids` is exactly the set alive at `run_end()`. When it is, the next
		/// frame's arriving set is again a full node set and the fast path continues.
		bool gives_full_child = false;

		/// The characters that necessarily follow this edge's own, and the state they
		/// lead to.
		///
		/// A pattern's literal stretches are chains of states with one way in and one
		/// way out, and walking them one character at a time is one lookup and one call
		/// each. compile() collapses a chain into a run compared in a single pass, so
		/// /api/v1/g0/r0/ costs a comparison rather than fourteen transitions. This is
		/// what a radix tree gets from storing whole segments per node; the automaton
		/// can have it too, because collapsing changes only the representation.
		///
		/// A chain is only collapsed where nothing observable happens along it: no
		/// branch, no capture, no repeat limit, no pattern ending, and the same set of
		/// patterns alive throughout.
		std::vector<char_t> run;
		Node* run_to = nullptr;

		Node* run_end() const { return run.empty() ? to : run_to; }

		EdgeInfo() = default;
		EdgeInfo(const EdgeInfo& info) {
			for (auto x : info.paths) {
				if (x.second.has_value()) {
					paths[x.first] = x.second;
				} else {
					paths[x.first] = std::nullopt;
				}
			}
			for (auto x : info.tag_actions) {
				tag_actions[x.first] = x.second;
			}
			to = info.to;
			// Deliberately not copied. A copied edge belongs to a graph that is still
			// being built, and stale derived data is worse than none: compile() sets
			// it, and until then every edge takes the general path.
			ids.clear();
			has_limits = false;
			gives_full_child = false;
		}
		// Movable now, so edges can live in one run rather than in separately allocated
		// tree nodes. The copy constructor above stays as it was.
		EdgeInfo(EdgeInfo&&) noexcept = default;
		EdgeInfo& operator=(EdgeInfo&&) noexcept = default;
	};

	template <typename char_t>
	struct symbol {
		char_t ch;
		bool wildcard;
		bool none;

		/// Non-zero when this symbol stands for a character class rather than a single
		/// character, and then it identifies which one. The members live on the node.
		///
		/// A class used to be built as one node per member, because a node's identity
		/// is the character it consumes. [A-Za-z0-9_.%-] is 66 of them, and a repeat
		/// over it connects every member to every member: 66 nodes and 4356 edges for
		/// one route parameter, repeated per route because nothing is shared between
		/// prefixes. A thousand parameterised routes came to 4,490,138 edges and 1.26GB.
		///
		/// Each occurrence gets its own id, so two different classes leaving the same
		/// node stay distinct edges rather than colliding on one key.
		std::uint32_t cls = 0;

		static const symbol Any;
		static const symbol None;
		static const symbol EOR;  // end-of-regex

		symbol() : symbol(symbol<char_t>::None) {}
		symbol(char_t s) : ch(s), wildcard(false), none(false) {}
		symbol(char_t s, bool w, bool n) : ch(s), wildcard(w), none(n) {}

		static symbol of_class(std::uint32_t id) {
			symbol s{char_t{}, false, false};
			s.cls = id;
			return s;
		}

		bool is_class() const { return cls != 0; }

		inline bool operator==(const symbol& s) const {
			return (wildcard == s.wildcard) && (none == s.none) && (ch == s.ch) && (cls == s.cls);
		}
		inline bool operator!=(const symbol<char_t>& s) const { return !(*this == s); }

		bool operator<(const symbol<char_t>& s) const {
			if (cls != s.cls) {
				return cls < s.cls;
			}
			if (ch == s.ch) {
				if (wildcard == s.wildcard) {
					return none < s.none;
				}
				return wildcard < s.wildcard;
			}
			return (ch < s.ch);
		}

		inline std::basic_string<char_t> to_string() const {
			if (is_class()) {
				return "class";
			}
			if (*this == symbol::Any) {
				return "wildcard";
			}
			if (*this == symbol::None) {
				return "(empty)";
			}
			if (*this == symbol::EOR) {
				return "EOR";
			}
			return std::basic_string<char_t>(1, ch);
		}
	};

	template <typename char_t>
	const symbol<char_t> symbol<char_t>::Any{'\0', true, false};

	template <typename char_t>
	const symbol<char_t> symbol<char_t>::None{'-', false, true};

	template <typename char_t>
	const symbol<char_t> symbol<char_t>::EOR{'\0', false, true};

	template <typename RegexData, typename char_t>
	class Node {
		friend class matcher::RegexMatcher<RegexData, char_t>;
		friend struct EdgeInfo<RegexData, Node<RegexData, char_t>, char_t>;

		static std::map<symbol<char_t>, std::string> special_symbols;

		/**
		 * @brief All directly connected nodes
		 *
		 */
		flat_map<symbol<char_t>, EdgeInfo<RegexData, Node<RegexData, char_t>, char_t>> neighbours;

		/**
		 * @brief The current symbol that the regex would match
		 *
		 */
		symbol<char_t> current_symbol;

		/// Everything to do with character classes, allocated only where there is any.
		///
		/// Two empty vectors on every node cost forty bytes apiece, and a table of ten
		/// thousand routes has hundreds of thousands of nodes of which a handful touch a
		/// class at all. Behind one pointer it is eight bytes on the nodes that do not.
		struct ClassData {
			/// The characters this node accepts, sorted, when its symbol is a class. One
			/// node stands for the whole class instead of one per member.
			std::vector<char_t> members;

			/// The class edges leaving this node, gathered by compile() so matching does
			/// not have to scan every neighbour looking for them.
			std::vector<const EdgeInfo<RegexData, Node<RegexData, char_t>, char_t>*> edges;
		};
		std::unique_ptr<ClassData> class_data;

		ClassData& ensure_class_data() {
			if (!class_data) {
				class_data = std::make_unique<ClassData>();
			}
			return *class_data;
		}

		const std::vector<char_t>& class_members() const {
			static const std::vector<char_t> none;
			return class_data ? class_data->members : none;
		}

		bool accepts(char_t c) const {
			return class_data && std::binary_search(class_data->members.begin(), class_data->members.end(), c);
		}

		/// Whether this node has a wildcard edge or an epsilon edge at all.
		///
		/// Every character used to cost three lookups: the character itself, then Any,
		/// then None. Almost no node has either, so two of the three found nothing and
		/// were pure cost. Set by compile().
		bool has_any_edge = false;
		bool has_none_edge = false;

		/// Whether compile() has run over this node. False means every edge takes the
		/// general path, which is what the matcher did before any of this existed.
		///
		/// The set of regexes alive at a node is what compile() reasons about, but it
		/// is not kept: only the per-edge answer derived from it is needed at match
		/// time, and along a shared prefix that set is the whole route table, so
		/// storing one per node would have cost more memory than the structure it was
		/// speeding up.
		bool compiled = false;

	public:
		/**
		 * @brief Construct a new Node object
		 *
		 */
		Node() { current_symbol = symbol<char_t>::None; }

		/**
		 * @brief Construct a new Node object
		 *
		 * @param ch Node's symbol<char_t>
		 */
		Node(symbol<char_t> ch) { current_symbol = ch; }

	private:
		/**
		 * @brief Represents the current node's symbol as string
		 *
		 * @return std::string string representation of the node's symbol<char_t>
		 */
		std::string to_string() const {
			if (current_symbol == symbol<char_t>::Any) {
				return "wildcard";
			}
			if (current_symbol == symbol<char_t>::None) {
				return "(empty)";
			}
			return std::string(1, current_symbol.ch);
		}

		/**
		 * @brief Attaches this to the children of with
		 *
		 * @param with Node which's children are being attached to this
		 */
		void absorb(Node<RegexData, char_t>* with);

		/**
		 * @brief Checks if there is a node with a given symbol in the neighbour list
		 *
		 * @param ch Symbol to be checked
		 * @return true if a node with this symbol is direct neighbour to this node
		 * @return false if there is no node with this symbol as direct neighbour to this node
		 */
		bool hasChild(symbol<char_t> ch) const;

		/**
		 * @brief Get the Child object with the given symbol
		 *
		 * @param ch the symbol that's being looked for
		 * @return Node* the node correspondign to this symbol
		 */
		Node<RegexData, char_t>* getChild(symbol<char_t> ch);

		/**
		 * @brief Adds a child node to the current one and marks the connection as part of a given regex match
		 *
		 * @param child  Existing node
		 * @param regex  Regex data that is being used to indentify the regex that the edge is part of
		 * @param limits Pointer to the shared limit of the edge (nullptr if no limit is applied)
		 */
		void connect_with(Node<RegexData, char_t>* child, RegexData regex,
		                  std::vector<std::unique_ptr<Limits>>& limits_storage,
		                  std::optional<Limits*> limits = std::nullopt);

		void connect_with(Node<RegexData, char_t>* child, RegexData regex, const std::vector<TagAction>& actions,
		                  std::vector<std::unique_ptr<Limits>>& limits_storage,
		                  std::optional<Limits*> limits = std::nullopt);

		/**
		 * @brief Matches a string with all regexes and returns the identified of the one that matches
		 *
		 * @param text string that is being tried to be matched with any of the added regexes
		 * @tparam ConstIterator const iterator in a container
		 * @return std::vector<RegexData> set of unique identifiers of the regexes that matches the string
		 */
		template <typename ConstIterator>
		std::vector<RegexData> match(ConstIterator, ConstIterator) const;

		template <typename ConstIterator>
		std::vector<RegexData> match_helper(ConstIterator, ConstIterator, const std::vector<RegexData>&, const Node*,
		                                    LimitState&, bool paths_is_live = false) const;

		/**
		 * @brief Matches a string with all regexes and returns matches with captured groups
		 *
		 * @param begin Start iterator of the string
		 * @param end End iterator of the string
		 * @tparam ConstIterator const iterator in a container
		 * @return std::vector<matcher::MatchResult<RegexData>> matches with group captures
		 */
		template <typename ConstIterator>
		std::vector<matcher::MatchResult<RegexData>> match_with_groups(ConstIterator begin, ConstIterator end) const;

		// paths_is_live says the arriving set is exactly this node's `live` set, which
		// is what lets a step skip rebuilding it. False is always safe: it only costs
		// the general path.
		template <typename ConstIterator>
		void match_with_groups_helper(ConstIterator begin, ConstIterator end, size_t position,
		                              const std::vector<RegexData>& paths, const Node* prev,
		                              CaptureTable<RegexData>& capture_slots, LimitState& limit_state,
		                              std::vector<matcher::MatchResult<RegexData>>& results,
		                              bool paths_is_live = false) const;

#ifdef DEBUG
		void print_helper(size_t layer, std::set<const Node<RegexData, char_t>*>& traversed,
		                  std::map<const Node<RegexData, char_t>*, std::string>& nodes) const;

		void print() const;
#endif
	};

	template <typename Node_T>
	class SubTree {
	public:
		std::vector<Node_T*> roots;
		std::vector<Node_T*> leafs;

		SubTree(std::vector<Node_T*> a, std::vector<Node_T*> b) : roots(a), leafs(b) {}
		SubTree() : roots(), leafs() {}

		inline const std::vector<Node_T*>& get_roots() const;

		inline const std::vector<Node_T*>& get_leafs() const;
	};
}  // namespace

#include <matcher/impl/subtree.ipp>
#include <matcher/impl/node.ipp>

namespace matcher {
	template <typename RegexData, typename char_t>
	class RegexMatcher {
		Node<RegexData, char_t> root;
		std::vector<std::unique_ptr<Node<RegexData, char_t>>> nodes_storage;
		std::vector<std::unique_ptr<Limits>> limits_storage;
		/// Hands each character class its own identity, so two different classes leaving
		/// one node are two edges rather than one.
		std::uint32_t class_counter = 0;

		template <typename ConstIterator>
		static Limits* processLimit(const SubTree<Node<RegexData, char_t>>&, SubTree<Node<RegexData, char_t>>&,
		                            RegexData, ConstIterator&, std::vector<std::unique_ptr<Limits>>&);

		template <typename ConstIterator>
		static SubTree<Node<RegexData, char_t>> processSet(std::vector<Node<RegexData, char_t>*>, RegexData,
		                                                   ConstIterator&,
		                                                   std::vector<std::unique_ptr<Node<RegexData, char_t>>>&,
		                                                   std::uint32_t& class_counter);

		template <typename ConstIterator>
		static SubTree<Node<RegexData, char_t>> process(std::vector<Node<RegexData, char_t>*>, RegexData,
		                                                ConstIterator&, ConstIterator, const bool,
		                                                size_t& group_counter, std::vector<TagAction>& pending_actions,
		                                                std::vector<std::unique_ptr<Node<RegexData, char_t>>>&,
		                                                std::vector<std::unique_ptr<Limits>>&,
		                                                std::uint32_t& class_counter);

	public:
		/**
		 * @brief Construct a new Regex Matcher object
		 *
		 */
		RegexMatcher() {}

		/**
		 * @brief Adds another regex to the set of regexes
		 *
		 * @tparam Iterable Set of characters for the regex. Must implement std::cbegin and std::cend
		 * @tparam RegexData Value that will be associated with the regex
		 */
		template <typename Iterable>
		void add_regex(Iterable, RegexData);

		/**
		 * @brief Precomputes what the walk would otherwise recompute per character
		 *
		 * Which regexes stay alive through an edge does not depend on the input, so it
		 * does not belong in the inner loop. This walks the graph once and records, per
		 * node, the set of regexes alive there, and per edge, whether taking it leaves
		 * that set whole. A match can then follow a deterministic path without touching
		 * the set at all, which is the difference between a lookup that costs one pass
		 * over the route table per character and one that does not.
		 *
		 * Explicit rather than lazy, on purpose. Doing it on the first match would mean
		 * mutating the graph from a const call that several threads may be in at once,
		 * which is the shape of bug this matcher has just had fixed once. add_regex is
		 * already a build-time operation and matching is already const, so "build, then
		 * compile, then share" is the contract the class already had; this only names
		 * the middle step.
		 *
		 * Matching without it is correct and takes the general path, which is what the
		 * matcher did before this existed. Safe to call repeatedly, and cheap when
		 * nothing has changed.
		 */
		void compile();

		/**
		 * @brief Matches a string with all added regexes
		 *
		 * @tparam Iterable Set of characters. Must implement std::cbegin and std::cend
		 * @return std::vector<RegexData> Set of regexes' UIDs that match the text
		 */
		template <typename Iterable>
		std::vector<RegexData> match(Iterable) const;

		/**
		 * @brief Matches a string with all added regexes and returns captured groups
		 *
		 * @tparam Iterable Set of characters. Must implement std::cbegin and std::cend
		 * @return std::vector<MatchResult<RegexData>> Set of matches with captured group positions
		 */
		template <typename Iterable>
		std::vector<MatchResult<RegexData>> match_with_groups(Iterable) const;

#ifdef DEBUG
		/**
		 * @brief Prints list of edges withing the pattern graph
		 *
		 */
		void print() const { this->root.print(); }
#endif
	};
}  // namespace matcher

#include <matcher/impl/core.ipp>
