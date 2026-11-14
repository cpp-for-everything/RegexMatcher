/**
 * @file dynamic.ipp
 * @brief Implementation of matcher::DynamicSet (dynamic.hpp)
 * @note This file is included by dynamic.hpp - do not include directly
 *
 * Names follow theory/proofs.tex of paper p10: a record is a state of D_P; init is the initial record;
 * the in-degree counter of a record counts the cells that point to it plus the virtual initial edge;
 * LiveSet[j] is the set of records whose key holds pattern j; C is the candidate set of an addition.
 *
 * With MATCHER_DYNAMIC_CHECKED defined, every arena access is bounds checked and a failed check throws
 * std::logic_error. The tests define it. The reserved arena of the slot tagged layout is invisible to
 * AddressSanitizer, so these checks are the only guard there.
 */

#include <algorithm>
#include <exception>
#include <limits>
#include <numeric>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define MATCHER_DYNAMIC_UNDEF_LEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#define MATCHER_DYNAMIC_UNDEF_NOMINMAX
#endif
#include <windows.h>
#ifdef MATCHER_DYNAMIC_UNDEF_LEAN
#undef WIN32_LEAN_AND_MEAN
#undef MATCHER_DYNAMIC_UNDEF_LEAN
#endif
#ifdef MATCHER_DYNAMIC_UNDEF_NOMINMAX
#undef NOMINMAX
#undef MATCHER_DYNAMIC_UNDEF_NOMINMAX
#endif
#else
#include <sys/mman.h>
#endif

#ifdef MATCHER_DYNAMIC_CHECKED
#define MATCHER_DYNAMIC_CHECK(cond, what)                 \
	do {                                                  \
		if (!(cond)) {                                    \
			throw std::logic_error("DynamicSet: " what);  \
		}                                                 \
	} while (0)
#else
#define MATCHER_DYNAMIC_CHECK(cond, what) \
	do {                                  \
	} while (0)
#endif

namespace matcher {

	// ------------------------------------------------------------------ DFA helpers

	inline void validate_dfa(const MinimalDfa& dfa) {
		if (dfa.state_count == 0) {
			throw std::invalid_argument("MinimalDfa: no state");
		}
		if (dfa.class_count == 0 || dfa.class_count > 256) {
			throw std::invalid_argument("MinimalDfa: class_count must be 1 to 256");
		}
		for (size_t b = 0; b < 256; b++) {
			if (dfa.byte_class[b] >= dfa.class_count) {
				throw std::invalid_argument("MinimalDfa: byte class out of range");
			}
		}
		if (dfa.delta.size() != size_t(dfa.state_count) * dfa.class_count) {
			throw std::invalid_argument("MinimalDfa: delta must have state_count * class_count entries");
		}
		for (const uint32_t t : dfa.delta) {
			if (t >= dfa.state_count) {
				throw std::invalid_argument("MinimalDfa: transition target out of range");
			}
		}
		if (dfa.accepting.size() != dfa.state_count) {
			throw std::invalid_argument("MinimalDfa: accepting must have state_count entries");
		}
		if (dfa.initial >= dfa.state_count) {
			throw std::invalid_argument("MinimalDfa: initial state out of range");
		}
		if (dfa.dead != MinimalDfa::none && dfa.dead >= dfa.state_count) {
			throw std::invalid_argument("MinimalDfa: dead state out of range");
		}
	}

	namespace detail {
		struct VectorHash {
			size_t operator()(const std::vector<uint32_t>& v) const noexcept {
				uint64_t h = 1469598103934665603ull;
				for (const uint32_t x : v) {
					h = (h ^ x) * 1099511628211ull;
				}
				return static_cast<size_t>(h ^ (h >> 29));
			}
		};

		// The dead state of a complete DFA: the state that reaches no accepting state. Throws when there
		// is more than one (the DFA is then not minimal) or when it is not absorbing.
		inline uint32_t find_dead_state(const MinimalDfa& dfa) {
			const size_t n = dfa.state_count;
			const size_t m = dfa.class_count;
			std::vector<std::vector<uint32_t>> rev(n);
			for (size_t s = 0; s < n; s++) {
				for (size_t c = 0; c < m; c++) {
					rev[dfa.delta[s * m + c]].push_back(static_cast<uint32_t>(s));
				}
			}
			std::vector<uint8_t> good(n, 0);
			std::vector<uint32_t> stack;
			for (size_t s = 0; s < n; s++) {
				if (dfa.accepting[s]) {
					good[s] = 1;
					stack.push_back(static_cast<uint32_t>(s));
				}
			}
			while (!stack.empty()) {
				const uint32_t t = stack.back();
				stack.pop_back();
				for (const uint32_t s : rev[t]) {
					if (!good[s]) {
						good[s] = 1;
						stack.push_back(s);
					}
				}
			}
			uint32_t dead = MinimalDfa::none;
			for (size_t s = 0; s < n; s++) {
				if (!good[s]) {
					if (dead != MinimalDfa::none) {
						throw std::invalid_argument("MinimalDfa: more than one dead state (not minimal)");
					}
					dead = static_cast<uint32_t>(s);
				}
			}
			if (dead != MinimalDfa::none) {
				for (size_t c = 0; c < m; c++) {
					if (dfa.delta[size_t(dead) * m + c] != dead) {
						throw std::invalid_argument("MinimalDfa: dead state is not absorbing");
					}
				}
			}
			return dead;
		}
	}  // namespace detail

	inline MinimalDfa minimize_dfa(const MinimalDfa& in) {
		validate_dfa(in);
		const size_t m = in.class_count;
		constexpr uint32_t none = MinimalDfa::none;
		// accessible part, in breadth first order
		std::vector<uint32_t> order;
		std::vector<uint32_t> index(in.state_count, none);
		index[in.initial] = 0;
		order.push_back(in.initial);
		for (size_t head = 0; head < order.size(); head++) {
			const uint32_t s = order[head];
			for (size_t c = 0; c < m; c++) {
				const uint32_t t = in.delta[size_t(s) * m + c];
				if (index[t] == none) {
					index[t] = static_cast<uint32_t>(order.size());
					order.push_back(t);
				}
			}
		}
		const size_t n = order.size();
		std::vector<uint32_t> delta(n * m);
		std::vector<uint8_t> acc(n);
		for (size_t i = 0; i < n; i++) {
			for (size_t c = 0; c < m; c++) {
				delta[i * m + c] = index[in.delta[size_t(order[i]) * m + c]];
			}
			acc[i] = in.accepting[order[i]] ? 1 : 0;
		}
		// Moore's algorithm: refine blocks by (block, blocks of the successors) until stable
		std::vector<uint32_t> block(n);
		size_t blocks = 0;
		{
			uint32_t id_of[2] = {none, none};
			for (size_t i = 0; i < n; i++) {
				if (id_of[acc[i]] == none) {
					id_of[acc[i]] = static_cast<uint32_t>(blocks++);
				}
				block[i] = id_of[acc[i]];
			}
		}
		std::vector<uint32_t> next_block(n);
		std::vector<uint32_t> sig(m + 1);
		while (true) {
			std::unordered_map<std::vector<uint32_t>, uint32_t, detail::VectorHash> ids;
			ids.reserve(blocks * 2 + 1);
			for (size_t i = 0; i < n; i++) {
				sig[0] = block[i];
				for (size_t c = 0; c < m; c++) {
					sig[c + 1] = block[delta[i * m + c]];
				}
				const auto found = ids.emplace(sig, static_cast<uint32_t>(ids.size())).first;
				next_block[i] = found->second;
			}
			const size_t now = ids.size();
			block.swap(next_block);
			if (now == blocks) {
				break;
			}
			blocks = now;
		}
		// renumber blocks in breadth first order from the initial state
		std::vector<uint32_t> rep(blocks, none);
		for (size_t i = 0; i < n; i++) {
			if (rep[block[i]] == none) {
				rep[block[i]] = static_cast<uint32_t>(i);
			}
		}
		std::vector<uint32_t> new_id(blocks, none);
		std::vector<uint32_t> border;
		new_id[block[0]] = 0;
		border.push_back(block[0]);
		for (size_t head = 0; head < border.size(); head++) {
			const uint32_t b = border[head];
			for (size_t c = 0; c < m; c++) {
				const uint32_t bt = block[delta[size_t(rep[b]) * m + c]];
				if (new_id[bt] == none) {
					new_id[bt] = static_cast<uint32_t>(border.size());
					border.push_back(bt);
				}
			}
		}
		MinimalDfa out;
		out.state_count = static_cast<uint32_t>(border.size());
		out.class_count = in.class_count;
		out.byte_class = in.byte_class;
		out.delta.resize(size_t(out.state_count) * m);
		out.accepting.resize(out.state_count);
		for (size_t i = 0; i < border.size(); i++) {
			const uint32_t r = rep[border[i]];
			for (size_t c = 0; c < m; c++) {
				out.delta[i * m + c] = new_id[block[delta[size_t(r) * m + c]]];
			}
			out.accepting[i] = acc[r];
		}
		out.initial = 0;
		out.dead = detail::find_dead_state(out);
		return out;
	}

	namespace detail {
		struct RegexAutomatonAccess {
			// Subset construction of the library's automaton for one pattern, complete (the empty set is the
			// sink), over the byte classes of the library's ByteMap. Same semantics as the lookup table.
			template <typename Iterable>
			static MinimalDfa subset(const Iterable& pattern, size_t max_states) {
				using Kind = State::Kind;
				RegexMatcher<uint32_t, char> m;
				m.add_regex(pattern, 0u);
				const ByteMap map = m.byte_map();
				MinimalDfa d;
				d.class_count = static_cast<uint32_t>(map.class_count);
				for (size_t b = 0; b < 256; b++) {
					d.byte_class[b] = map.byte_class[b];
				}
				ClosureScratch scratch;
				std::unordered_map<std::vector<uint32_t>, uint32_t, StateSetHash> index;
				std::vector<std::vector<uint32_t>> sets;
				const auto add = [&](std::vector<uint32_t>&& set) {
					const auto found = index.find(set);
					if (found != index.end()) {
						return found->second;
					}
					if (sets.size() >= max_states) {
						throw std::length_error("compile_regex_dfa: DFA passes max_states");
					}
					const uint32_t id = static_cast<uint32_t>(sets.size());
					index.emplace(set, id);
					sets.push_back(std::move(set));
					return id;
				};
				std::vector<uint32_t> set;
				m.closure({0}, set, scratch);
				add(std::move(set));
				const size_t k = map.class_count;
				std::vector<std::vector<uint32_t>> buckets(k);
				for (size_t current = 0; current < sets.size(); current++) {
					const std::vector<uint32_t> members = sets[current];
					for (auto& bucket : buckets) {
						bucket.clear();
					}
					for (const uint32_t s : members) {
						const State& state = m.states[s];
						if (state.kind != Kind::Char) {
							continue;
						}
						for (const uint16_t c : map.accepts[state.value]) {
							buckets[c].push_back(state.out[0]);
						}
					}
					for (size_t c = 0; c < k; c++) {
						std::vector<uint32_t> target;
						if (!buckets[c].empty()) {
							m.closure(buckets[c], target, scratch);
						}
						d.delta.push_back(add(std::move(target)));
					}
				}
				d.state_count = static_cast<uint32_t>(sets.size());
				d.accepting.resize(sets.size(), 0);
				for (size_t i = 0; i < sets.size(); i++) {
					for (const uint32_t s : sets[i]) {
						if (m.states[s].kind == Kind::Accept) {
							d.accepting[i] = 1;
						}
					}
				}
				d.initial = 0;
				return d;
			}
		};
	}  // namespace detail

	template <typename Iterable>
	MinimalDfa compile_regex_dfa(const Iterable& pattern, size_t max_states) {
		return minimize_dfa(detail::RegexAutomatonAccess::subset(pattern, max_states));
	}

	namespace detail {
		constexpr uint32_t no_record = 0xFFFFFFFFu;

		// One live component of a key. pos is the position of the record in LiveSet[pid]; it is not part
		// of the key's identity.
		struct KeyEntry {
			uint32_t pid;
			uint32_t comp;
			uint32_t pos;
		};

		inline uint64_t mix64(uint64_t x) {
			x ^= x >> 33;
			x *= 0xff51afd7ed558ccdull;
			x ^= x >> 33;
			x *= 0xc4ceb9fe1a85ec53ull;
			x ^= x >> 33;
			return x;
		}
		// The hash of a key is a left fold over its entries, so appending the newest pattern costs O(1).
		constexpr uint64_t key_seed = 0x9E3779B97F4A7C15ull;
		inline uint64_t key_step(uint64_t h, uint32_t pid, uint32_t comp) {
			return mix64(h ^ ((uint64_t(pid) << 32) | comp)) + 0x632BE59BD9B4E019ull;
		}

		// Map from (record, state of A) to the new record, cleared in O(1) by a generation number
		struct PairTable {
			struct Slot {
				uint64_t key;
				uint32_t value;
				uint32_t gen;
			};
			std::vector<Slot> slots;
			size_t mask = 0;
			size_t count = 0;
			uint32_t gen = 1;

			void clear() {
				count = 0;
				if (++gen == 0) {
					for (auto& s : slots) {
						s.gen = 0;
					}
					gen = 1;
				}
			}
			uint32_t find(uint64_t key) const {
				if (slots.empty()) {
					return no_record;
				}
				for (size_t i = mix64(key) & mask;; i = (i + 1) & mask) {
					const Slot& s = slots[i];
					if (s.gen != gen) {
						return no_record;
					}
					if (s.key == key) {
						return s.value;
					}
				}
			}
			void insert(uint64_t key, uint32_t value) {
				if ((count + 1) * 2 > slots.size()) {
					grow();
				}
				for (size_t i = mix64(key) & mask;; i = (i + 1) & mask) {
					Slot& s = slots[i];
					if (s.gen != gen) {
						s = Slot{key, value, gen};
						count++;
						return;
					}
				}
			}
			size_t bytes() const { return slots.capacity() * sizeof(Slot); }

		private:
			void grow() {
				std::vector<Slot> old;
				old.swap(slots);
				const size_t size = old.empty() ? 1024 : old.size() * 2;
				slots.assign(size, Slot{0, 0, 0});
				mask = size - 1;
				const uint32_t g = gen;
				count = 0;
				for (const Slot& s : old) {
					if (s.gen == g) {
						insert(s.key, s.value);
					}
				}
			}
		};

		// Address space reserved once, committed on demand: the fixed slot bases of the slot tagged layout
		class ReservedCells {
		public:
			ReservedCells() = default;
			explicit ReservedCells(size_t cells) : cells_(cells) {
				const size_t bytes = cells * sizeof(uint32_t);
#if defined(_WIN32)
				base_ = static_cast<uint32_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE, PAGE_NOACCESS));
#else
				void* p = mmap(nullptr, bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
				base_ = p == MAP_FAILED ? nullptr : static_cast<uint32_t*>(p);
#endif
				if (base_ == nullptr) {
					throw std::bad_alloc();
				}
			}
			ReservedCells(ReservedCells&& o) noexcept : base_(o.base_), cells_(o.cells_) {
				o.base_ = nullptr;
				o.cells_ = 0;
			}
			ReservedCells& operator=(ReservedCells&& o) noexcept {
				if (this != &o) {
					release();
					base_ = o.base_;
					cells_ = o.cells_;
					o.base_ = nullptr;
					o.cells_ = 0;
				}
				return *this;
			}
			ReservedCells(const ReservedCells&) = delete;
			ReservedCells& operator=(const ReservedCells&) = delete;
			~ReservedCells() { release(); }

			uint32_t* base() const { return base_; }
			size_t cells() const { return cells_; }

			// Makes [first, first + count) cells readable and writable
			void commit(size_t first, size_t count) {
				if (count == 0) {
					return;
				}
				if (first + count > cells_) {
					throw std::length_error("DynamicSet: slot arena full");
				}
				uint32_t* p = base_ + first;
#if defined(_WIN32)
				if (VirtualAlloc(p, count * sizeof(uint32_t), MEM_COMMIT, PAGE_READWRITE) == nullptr) {
					throw std::bad_alloc();
				}
#else
				const size_t page = 4096;
				uintptr_t lo = reinterpret_cast<uintptr_t>(p) & ~(uintptr_t(page) - 1);
				uintptr_t hi = (reinterpret_cast<uintptr_t>(p + count) + page - 1) & ~(uintptr_t(page) - 1);
				if (mprotect(reinterpret_cast<void*>(lo), hi - lo, PROT_READ | PROT_WRITE) != 0) {
					throw std::bad_alloc();
				}
#endif
			}

		private:
			void release() noexcept {
				if (base_ != nullptr) {
#if defined(_WIN32)
					VirtualFree(base_, 0, MEM_RELEASE);
#else
					munmap(base_, cells_ * sizeof(uint32_t));
#endif
					base_ = nullptr;
				}
			}
			uint32_t* base_ = nullptr;
			size_t cells_ = 0;
		};

		using ClassMap = std::array<uint8_t, 256>;

		// ------------------------------------------------------------ L1: slot tagged epochs
		//
		// A cell holds (slot tag t: 4 bits, row index i: 28 bits) of its target. Slot t holds a partition
		// (one 8 bit field per slot in the class vector of each byte), a row width k_t and an arena at the
		// fixed address base + t * 2^S cells. The row of index i of slot t starts at cell i * k_t of its
		// arena. A record keeps its slot and its index for life; a slot recycle rewrites the rows of one
		// slot in a finer partition, in place, and no cell elsewhere changes.
		class SlotTaggedLayout {
		public:
			static constexpr uint32_t invalid_cell = 0xFFFFFFFFu;
			static constexpr uint32_t index_mask = 0x0FFFFFFFu;
			static constexpr uint32_t max_index = 0x0FFFFFFEu;

			void init(const DynamicOptions& o, uint32_t k, const ClassMap& cls) {
				if (o.slots < 1 || o.slots > 16) {
					throw std::invalid_argument("DynamicSet: slots must be 1 to 16");
				}
				if (o.slot_reserve_log2 < 12 || o.slot_reserve_log2 > 32) {
					throw std::invalid_argument("DynamicSet: slot_reserve_log2 must be 12 to 32");
				}
				slot_count_ = o.slots;
				shift_ = o.slot_reserve_log2;
				arena_ = ReservedCells(size_t(slot_count_) << shift_);
				cur_ = 0;
				slots_[0].used = true;
				set_classes(0, cls);
				set_width(0, k);
			}
			void reserve_records(size_t n) {
				if (tag_.size() < n) {
					tag_.resize(n, 0);
					idx_.resize(n, 0);
				}
			}
			uint32_t encode(uint32_t rid) const { return (uint32_t(tag_[rid]) << 28) | idx_[rid]; }
			uint32_t decode(uint32_t cell) const {
				const Slot& s = slots_[cell >> 28];
				const uint32_t i = cell & index_mask;
				MATCHER_DYNAMIC_CHECK(i < s.rid_of.size() && s.rid_of[i] != no_record, "dangling cell");
				return s.rid_of[i];
			}
			void alloc_row(uint32_t rid) {
				Slot& s = slots_[cur_];
				uint32_t i = 0;
				if (!s.free.empty()) {
					i = s.free.back();
					s.free.pop_back();
				} else {
					if (s.n > max_index) {
						throw std::length_error("DynamicSet: slot index space full");
					}
					ensure(cur_, size_t(s.n) + 1, s.k);
					s.rid_of.push_back(no_record);
					i = s.n++;
				}
				s.rid_of[i] = rid;
				s.live++;
				tag_[rid] = static_cast<uint8_t>(cur_);
				idx_[rid] = i;
			}
			void free_row(uint32_t rid) {
				const unsigned t = tag_[rid];
				Slot& s = slots_[t];
				const uint32_t i = idx_[rid];
				s.rid_of[i] = no_record;
				s.free.push_back(i);
				s.live--;
				if (s.live == 0 && t != cur_) {
					reset_slot(t);
				}
			}
			// The targets of rid for the classes of the current partition, read through rid's own map
			void read_row(uint32_t rid, const ClassMap& rep, uint32_t k, uint32_t* out) const {
				const unsigned t = tag_[rid];
				const uint32_t* r = row(t, idx_[rid]);
				if (t == cur_) {
					for (uint32_t c = 0; c < k; c++) {
						out[c] = decode(r[c]);
					}
				} else {
					for (uint32_t c = 0; c < k; c++) {
						out[c] = decode(r[class_of(t, rep[c])]);
					}
				}
			}
			void write_row(uint32_t rid, const uint32_t* targets, uint32_t k) {
				const unsigned t = tag_[rid];
				MATCHER_DYNAMIC_CHECK(t == cur_ && slots_[t].k == k, "row written outside the current slot");
				uint32_t* r = row(t, idx_[rid]);
				for (uint32_t c = 0; c < k; c++) {
					r[c] = encode(targets[c]);
				}
			}
			template <typename F>
			void for_each_cell(uint32_t rid, F&& f) const {
				const unsigned t = tag_[rid];
				const uint32_t* r = row(t, idx_[rid]);
				const uint32_t k = slots_[t].k;
				for (uint32_t c = 0; c < k; c++) {
					f(decode(r[c]));
				}
			}
			uint32_t target(uint32_t rid, uint8_t byte) const {
				const unsigned t = tag_[rid];
				return decode(row(t, idx_[rid])[class_of(t, byte)]);
			}
			uint32_t row_width(uint32_t rid) const { return slots_[tag_[rid]].k; }
			unsigned tag_of(uint32_t rid) const { return tag_[rid]; }

			// REFINE in this layout: take a free slot for the new partition (no row written), or recycle the
			// slot with the fewest live rows (its rows are rewritten in the new partition, as the dense
			// REFINE restricted to that slot, with one counter increment per extra child cell).
			template <typename Inc>
			void refine(const ClassMap& ncls, uint32_t nk, const ClassMap& nrep, Inc&& inc, UpdateStats& st) {
				for (unsigned t = 0; t < slot_count_; t++) {
					if (t != cur_ && !slots_[t].used) {
						Slot& s = slots_[t];
						s.used = true;
						set_classes(t, ncls);
						set_width(t, nk);
						switch_current(t);
						return;
					}
				}
				unsigned t = 0;
				for (unsigned u = 1; u < slot_count_; u++) {
					if (slots_[u].live < slots_[t].live) {
						t = u;
					}
				}
				Slot& s = slots_[t];
				const uint32_t ok = s.k;
				std::array<uint8_t, 256> pmap{};
				std::array<uint32_t, 256> children{};
				for (uint32_t c = 0; c < nk; c++) {
					pmap[c] = static_cast<uint8_t>(class_of(t, nrep[c]));
					children[pmap[c]]++;
				}
				ensure(t, s.n, nk);
				std::vector<uint32_t> old(ok);
				for (uint32_t i = s.n; i-- > 0;) {
					if (s.rid_of[i] == no_record) {
						continue;
					}
					const uint32_t* from = arena_.base() + (size_t(t) << shift_) + size_t(i) * ok;
					std::copy(from, from + ok, old.begin());
					uint32_t* to = arena_.base() + (size_t(t) << shift_) + size_t(i) * nk;
					for (uint32_t c = 0; c < nk; c++) {
						to[c] = old[pmap[c]];
					}
					for (uint32_t oc = 0; oc < ok; oc++) {
						if (children[oc] > 1) {
							inc(decode(old[oc]), children[oc] - 1);
						}
					}
					st.refine_rows_rewritten++;
					st.refine_cells_written += nk;
				}
				st.slot_recycles = 1;
				set_classes(t, ncls);
				set_width(t, nk);
				switch_current(t);
			}

			// One dependent memory read per byte: the cell. The class vector of the byte does not depend
			// on the state; widths and the arena stride are in registers.
			uint32_t walk(const uint8_t* p, const uint8_t* end, uint32_t cell, uint32_t sink) const {
				const uint32_t* const base = arena_.base();
				const uint64_t* const vec = vec_.data();
				const uint64_t klo = klo_;
				const uint64_t khi = khi_;
				const unsigned shift = shift_;
				for (; p != end; ++p) {
					const size_t b2 = size_t(*p) * 2;
					const uint64_t vlo = vec[b2];
					const uint64_t vhi = vec[b2 + 1];
					const uint32_t t = cell >> 28;
					const uint32_t i = cell & index_mask;
					const unsigned sh = (t & 7u) * 8u;
					const bool upper = (t & 8u) != 0;
					const uint64_t w = upper ? vhi : vlo;
					const uint64_t kw = upper ? khi : klo;
					const uint32_t c = static_cast<uint32_t>(w >> sh) & 255u;
					const uint32_t k = (static_cast<uint32_t>(kw >> sh) & 255u) + 1u;
					cell = base[(size_t(t) << shift) + size_t(i) * k + c];
					if (cell == sink) {
						break;
					}
				}
				return cell;
			}

			size_t table_bytes() const {
				size_t b = sizeof(vec_);
				for (unsigned t = 0; t < slot_count_; t++) {
					b += size_t(slots_[t].n) * slots_[t].k * sizeof(uint32_t);
				}
				return b;
			}
			size_t table_live_bytes() const {
				size_t b = sizeof(vec_);
				for (unsigned t = 0; t < slot_count_; t++) {
					b += size_t(slots_[t].live) * slots_[t].k * sizeof(uint32_t);
				}
				return b;
			}
			size_t other_bytes() const {
				size_t b = tag_.capacity() + idx_.capacity() * sizeof(uint32_t);
				for (unsigned t = 0; t < slot_count_; t++) {
					b += slots_[t].rid_of.capacity() * sizeof(uint32_t) + slots_[t].free.capacity() * sizeof(uint32_t);
				}
				return b;
			}
			size_t slots_in_use() const {
				size_t n = 0;
				for (unsigned t = 0; t < slot_count_; t++) {
					n += slots_[t].used ? 1 : 0;
				}
				return n;
			}
			// Layout-level consistency: live counts, back references, partitions of older slots refined by
			// the newest one
			std::string check(const std::vector<uint8_t>& alive) const {
				std::string bad;
				for (unsigned t = 0; t < slot_count_; t++) {
					const Slot& s = slots_[t];
					uint32_t live = 0;
					for (uint32_t i = 0; i < s.rid_of.size(); i++) {
						const uint32_t r = s.rid_of[i];
						if (r == no_record) {
							continue;
						}
						live++;
						if (r >= alive.size() || !alive[r] || tag_[r] != t || idx_[r] != i) {
							bad += "slot back reference wrong; ";
						}
					}
					if (live != s.live) {
						bad += "slot live count wrong; ";
					}
					if (s.used && t != cur_) {
						for (size_t a = 0; a < 256; a++) {
							for (size_t b = a + 1; b < 256; b++) {
								if (class_of(cur_, uint8_t(a)) == class_of(cur_, uint8_t(b)) &&
								    class_of(t, uint8_t(a)) != class_of(t, uint8_t(b))) {
									return bad + "newest partition does not refine an older slot; ";
								}
							}
						}
					}
				}
				return bad;
			}

		private:
			struct Slot {
				uint32_t k = 1;
				uint32_t n = 0;     // rows allocated (high water mark), holes included
				uint32_t live = 0;  // rows of live records
				bool used = false;
				size_t committed = 0;  // cells committed in this slot's arena
				std::vector<uint32_t> free;
				std::vector<uint32_t> rid_of;
			};

			uint32_t class_of(unsigned t, uint8_t b) const {
				return static_cast<uint32_t>(vec_[size_t(b) * 2 + (t >> 3)] >> ((t & 7u) * 8u)) & 255u;
			}
			void set_classes(unsigned t, const ClassMap& cls) {
				const unsigned sh = (t & 7u) * 8u;
				for (size_t b = 0; b < 256; b++) {
					uint64_t& w = vec_[b * 2 + (t >> 3)];
					w = (w & ~(uint64_t(255) << sh)) | (uint64_t(cls[b]) << sh);
				}
			}
			void set_width(unsigned t, uint32_t k) {
				slots_[t].k = k;
				uint64_t& w = (t & 8u) ? khi_ : klo_;
				const unsigned sh = (t & 7u) * 8u;
				w = (w & ~(uint64_t(255) << sh)) | (uint64_t(k - 1) << sh);
			}
			void reset_slot(unsigned t) {
				Slot& s = slots_[t];
				s.n = 0;
				s.live = 0;
				s.used = false;
				s.free.clear();
				s.rid_of.clear();
			}
			void switch_current(unsigned t) {
				const unsigned old = cur_;
				cur_ = t;
				if (old != t && slots_[old].live == 0) {
					reset_slot(old);
				}
			}
			void ensure(unsigned t, size_t rows, uint32_t k) {
				Slot& s = slots_[t];
				const size_t need = rows * k;
				if (need > (size_t(1) << shift_)) {
					throw std::length_error("DynamicSet: slot arena full (raise slot_reserve_log2)");
				}
				if (need > s.committed) {
					size_t target = std::max<size_t>(need, std::max<size_t>(s.committed * 2, size_t(1) << 14));
					target = std::min(target, size_t(1) << shift_);
					arena_.commit((size_t(t) << shift_) + s.committed, target - s.committed);
					s.committed = target;
				}
			}
			const uint32_t* row(unsigned t, uint32_t i) const {
				MATCHER_DYNAMIC_CHECK(t < slot_count_ && i < slots_[t].n &&
				                          (size_t(i) + 1) * slots_[t].k <= slots_[t].committed,
				                      "row out of the slot arena");
				return arena_.base() + (size_t(t) << shift_) + size_t(i) * slots_[t].k;
			}
			uint32_t* row(unsigned t, uint32_t i) {
				return const_cast<uint32_t*>(static_cast<const SlotTaggedLayout&>(*this).row(t, i));
			}

			unsigned slot_count_ = 16;
			unsigned shift_ = 26;
			unsigned cur_ = 0;
			ReservedCells arena_;
			std::array<uint64_t, 512> vec_{};  // byte b: words 2b and 2b+1, 8 bits per slot
			uint64_t klo_ = 0;                 // k_t - 1 for slots 0 to 7, 8 bits each
			uint64_t khi_ = 0;                 // slots 8 to 15
			std::array<Slot, 16> slots_{};
			std::vector<uint8_t> tag_;
			std::vector<uint32_t> idx_;
		};

		// ------------------------------------------------------------ L2: dense rows, spare capacity
		//
		// One global partition. Rows are indexed by record and have a power of two stride >= k. A cell holds
		// the target's row offset (record << shift), so a step is cell = rows[cell + class[byte]]. A split
		// writes one new cell per new class in every row; a split past the stride moves every row.
		class DenseSpareLayout {
		public:
			static constexpr uint32_t invalid_cell = 0xFFFFFFFFu;

			void init(const DynamicOptions&, uint32_t k, const ClassMap& cls) {
				k_ = k;
				cls_ = cls;
				shift_ = 0;
				while ((uint32_t(1) << shift_) < k) {
					shift_++;
				}
			}
			void reserve_records(size_t n) {
				if (n <= cap_) {
					return;
				}
				size_t cap = std::max<size_t>(n, std::max<size_t>(cap_ * 2, 64));
				if ((uint64_t(cap) << shift_) >= (uint64_t(1) << 32)) {
					cap = std::max<size_t>(n, size_t((uint64_t(1) << 32) >> shift_) - 1);
					if ((uint64_t(cap) << shift_) >= (uint64_t(1) << 32)) {
						throw std::length_error("DynamicSet: dense table past 2^32 cells");
					}
				}
				rows_.resize(cap << shift_, 0);
				cap_ = cap;
			}
			uint32_t encode(uint32_t rid) const { return rid << shift_; }
			uint32_t decode(uint32_t cell) const { return cell >> shift_; }
			void alloc_row(uint32_t) {}
			void free_row(uint32_t) {}
			void read_row(uint32_t rid, const ClassMap&, uint32_t k, uint32_t* out) const {
				const uint32_t* r = row(rid);
				for (uint32_t c = 0; c < k; c++) {
					out[c] = r[c] >> shift_;
				}
			}
			void write_row(uint32_t rid, const uint32_t* targets, uint32_t k) {
				MATCHER_DYNAMIC_CHECK(k == k_, "row width");
				uint32_t* r = row(rid);
				for (uint32_t c = 0; c < k; c++) {
					r[c] = targets[c] << shift_;
				}
			}
			template <typename F>
			void for_each_cell(uint32_t rid, F&& f) const {
				const uint32_t* r = row(rid);
				for (uint32_t c = 0; c < k_; c++) {
					f(r[c] >> shift_);
				}
			}
			uint32_t target(uint32_t rid, uint8_t byte) const { return row(rid)[cls_[byte]] >> shift_; }
			uint32_t row_width(uint32_t) const { return k_; }

			template <typename ForEachLive, typename Inc>
			void refine(const ClassMap& ncls, uint32_t nk, const ClassMap& parent, ForEachLive&& for_each_live,
			            Inc&& inc, UpdateStats& st) {
				const uint32_t ok = k_;
				uint32_t nshift = shift_;
				while ((uint32_t(1) << nshift) < nk) {
					nshift++;
				}
				if (nshift != shift_) {
					if ((uint64_t(cap_) << nshift) >= (uint64_t(1) << 32)) {
						throw std::length_error("DynamicSet: dense table past 2^32 cells");
					}
					std::vector<uint32_t> moved(cap_ << nshift, 0);
					for_each_live([&](uint32_t rid) {
						const uint32_t* from = rows_.data() + (size_t(rid) << shift_);
						uint32_t* to = moved.data() + (size_t(rid) << nshift);
						for (uint32_t c = 0; c < ok; c++) {
							to[c] = (from[c] >> shift_) << nshift;
						}
						for (uint32_t c = ok; c < nk; c++) {
							to[c] = to[parent[c]];
							inc(to[c] >> nshift, 1);
						}
						st.refine_rows_rewritten++;
						st.refine_cells_written += nk;
					});
					rows_.swap(moved);
					shift_ = nshift;
				} else {
					for_each_live([&](uint32_t rid) {
						uint32_t* r = rows_.data() + (size_t(rid) << shift_);
						for (uint32_t c = ok; c < nk; c++) {
							r[c] = r[parent[c]];
							inc(r[c] >> shift_, 1);
						}
						st.refine_rows_rewritten++;
						st.refine_cells_written += nk - ok;
					});
				}
				k_ = nk;
				cls_ = ncls;
			}

			uint32_t walk(const uint8_t* p, const uint8_t* end, uint32_t cell, uint32_t sink) const {
				const uint32_t* const rows = rows_.data();
				const uint8_t* const cls = cls_.data();
				for (; p != end; ++p) {
					cell = rows[cell + cls[*p]];
					if (cell == sink) {
						break;
					}
				}
				return cell;
			}

			size_t table_bytes() const { return rows_.size() * sizeof(uint32_t) + sizeof(cls_); }
			size_t table_live_bytes(size_t live) const {
				return (live << shift_) * sizeof(uint32_t) + sizeof(cls_);
			}
			size_t other_bytes() const { return 0; }
			size_t slots_in_use() const { return 0; }

		private:
			const uint32_t* row(uint32_t rid) const {
				MATCHER_DYNAMIC_CHECK(rid < cap_, "row out of the dense table");
				return rows_.data() + (size_t(rid) << shift_);
			}
			uint32_t* row(uint32_t rid) { return const_cast<uint32_t*>(static_cast<const DenseSpareLayout&>(*this).row(rid)); }

			std::vector<uint32_t> rows_;
			size_t cap_ = 0;
			uint32_t shift_ = 0;
			uint32_t k_ = 1;
			ClassMap cls_{};
		};

		// ------------------------------------------------------------ L3: 256 columns, no classes
		class FullBytesLayout {
		public:
			static constexpr uint32_t invalid_cell = 0xFFFFFFFFu;

			void init(const DynamicOptions&, uint32_t k, const ClassMap&) {
				if (k != 256) {
					throw std::logic_error("FullBytesLayout needs the identity partition");
				}
			}
			void reserve_records(size_t n) {
				if (n <= cap_) {
					return;
				}
				const size_t cap = std::max<size_t>(n, std::max<size_t>(cap_ * 2, 64));
				if ((uint64_t(cap) << 8) >= (uint64_t(1) << 32)) {
					throw std::length_error("DynamicSet: table past 2^32 cells");
				}
				rows_.resize(cap << 8, 0);
				cap_ = cap;
			}
			uint32_t encode(uint32_t rid) const { return rid << 8; }
			uint32_t decode(uint32_t cell) const { return cell >> 8; }
			void alloc_row(uint32_t) {}
			void free_row(uint32_t) {}
			void read_row(uint32_t rid, const ClassMap&, uint32_t k, uint32_t* out) const {
				const uint32_t* r = row(rid);
				for (uint32_t c = 0; c < k; c++) {
					out[c] = r[c] >> 8;
				}
			}
			void write_row(uint32_t rid, const uint32_t* targets, uint32_t k) {
				uint32_t* r = const_cast<uint32_t*>(row(rid));
				for (uint32_t c = 0; c < k; c++) {
					r[c] = targets[c] << 8;
				}
			}
			template <typename F>
			void for_each_cell(uint32_t rid, F&& f) const {
				const uint32_t* r = row(rid);
				for (uint32_t c = 0; c < 256; c++) {
					f(r[c] >> 8);
				}
			}
			uint32_t target(uint32_t rid, uint8_t byte) const { return row(rid)[byte] >> 8; }
			uint32_t row_width(uint32_t) const { return 256; }
			template <typename ForEachLive, typename Inc>
			void refine(const ClassMap&, uint32_t, const ClassMap&, ForEachLive&&, Inc&&, UpdateStats&) {
				throw std::logic_error("FullBytesLayout never refines");
			}
			uint32_t walk(const uint8_t* p, const uint8_t* end, uint32_t cell, uint32_t sink) const {
				const uint32_t* const rows = rows_.data();
				for (; p != end; ++p) {
					cell = rows[cell + *p];
					if (cell == sink) {
						break;
					}
				}
				return cell;
			}
			size_t table_bytes() const { return rows_.size() * sizeof(uint32_t); }
			size_t table_live_bytes(size_t live) const { return live * 256 * sizeof(uint32_t); }
			size_t other_bytes() const { return 0; }
			size_t slots_in_use() const { return 0; }

		private:
			const uint32_t* row(uint32_t rid) const {
				MATCHER_DYNAMIC_CHECK(rid < cap_, "row out of the table");
				return rows_.data() + (size_t(rid) << 8);
			}
			std::vector<uint32_t> rows_;
			size_t cap_ = 0;
		};

		// ------------------------------------------------------------ the engine

		template <typename Id>
		class DynamicEngine {
		public:
			virtual ~DynamicEngine() = default;
			virtual UpdateStats add(uint32_t pid, const Id& id, MinimalDfa&& dfa) = 0;
			virtual UpdateStats remove(uint32_t pid) = 0;
			virtual uint32_t final_state(const uint8_t* text, size_t length) const = 0;
			virtual size_t state_count() const = 0;
			virtual size_t class_count() const = 0;
			virtual DynamicMemory memory() const = 0;
			virtual DynamicSnapshot<Id> snapshot() const = 0;
			virtual std::string check_invariants(bool deep) const = 0;
			virtual std::vector<uint32_t> records() const = 0;
			virtual uint32_t transition(uint32_t rid, uint8_t byte) const = 0;
			const std::vector<Id>& outputs_of(uint32_t rid) const { return out_[rid]; }

		protected:
			std::vector<std::vector<Id>> out_;  // output of each record, sorted ids
		};

		template <typename Id, typename Layout>
		class DynamicCore final : public DynamicEngine<Id> {
			using DynamicEngine<Id>::out_;

			struct Pattern {
				Id id;
				MinimalDfa dfa;
				std::vector<uint32_t> live;  // LiveSet[pid]
				bool active;
			};
			struct Item {
				uint32_t s;
				uint32_t t;
				uint32_t x;
			};

		public:
			DynamicCore(const DynamicOptions& options, bool identity_classes) : max_states_(options.max_states) {
				if (identity_classes) {
					k_ = 256;
					for (size_t b = 0; b < 256; b++) {
						cls_[b] = static_cast<uint8_t>(b);
						rep_[b] = static_cast<uint8_t>(b);
					}
				} else {
					k_ = 1;
					cls_.fill(0);
					rep_.fill(0);
				}
				lay_.init(options, k_, cls_);
				const uint32_t r0 = new_record({}, key_seed);
				std::vector<uint32_t> self(k_, r0);
				lay_.write_row(r0, self.data(), k_);
				indeg_[r0] = k_ + 1;  // its own cells plus the virtual initial edge
				init_ = r0;
			}

			// ------------------------------------------------------------ ADD
			UpdateStats add(uint32_t pid, const Id& id, MinimalDfa&& a) override {
				++op_;
				UpdateStats st;
				st.is_add = true;
				if (pats_.size() != pid) {
					throw std::logic_error("DynamicSet: pattern numbers out of order");
				}
				pats_.push_back(Pattern{id, std::move(a), {}, true});
				const MinimalDfa& A = pats_.back().dfa;
				// Step 1: the empty language changes nothing, and is tested before REFINE
				if (A.dead != MinimalDfa::none && A.initial == A.dead) {
					st.empty_language = true;
					finish(st);
					return st;
				}
				refine(A, st);  // step 1, REFINE
				const uint32_t k = k_;
				const uint32_t mA = A.class_count;
				const uint32_t deadA = A.dead;
				cmA_.resize(k);
				for (uint32_t c = 0; c < k; c++) {
					cmA_[c] = A.byte_class[rep_[c]];
				}
				pairs_.clear();
				cand_.clear();
				new_.clear();
				queue_.clear();
				std::exception_ptr error;
				try {
					// Steps 3 to 5: explore the live pairs only
					const auto new_pair = [&](uint32_t S, uint32_t T) -> uint32_t {
						const uint64_t pk = (uint64_t(S) << 32) | T;
						const uint32_t found = pairs_.find(pk);
						if (found != no_record) {
							return found;
						}
						if (records_ + 1 > max_states_) {
							throw std::length_error("DynamicSet: the automaton would pass max_states");
						}
						std::vector<KeyEntry> key = key_[S];
						key.push_back(KeyEntry{pid, T, 0});  // pid is the largest: the key stays sorted
						const uint64_t h = key_step(hash_[S], pid, T);
						const uint32_t x = new_record(std::move(key), h);
						pairs_.insert(pk, x);
						if (cand_op_[S] != op_) {
							cand_op_[S] = op_;
							cand_.push_back(S);
						}
						new_.push_back(x);
						queue_.push_back(Item{S, T, x});
						return x;
					};
					const uint32_t x0 = new_pair(init_, A.initial);
					sbuf_.resize(k);
					rbuf_.resize(k);
					for (size_t head = 0; head < queue_.size(); head++) {
						const Item it = queue_[head];
						lay_.read_row(it.s, rep_, k, sbuf_.data());
						st.old_rows_read++;
						const uint32_t* dT = A.delta.data() + size_t(it.t) * mA;
						for (uint32_t c = 0; c < k; c++) {
							const uint32_t S2 = sbuf_[c];
							const uint32_t T2 = dT[cmA_[c]];
							rbuf_[c] = (T2 == deadA) ? S2 : new_pair(S2, T2);
						}
						if (created_op_[it.x] != op_) {
							st.old_rows_written++;
						}
						lay_.write_row(it.x, rbuf_.data(), k);
						st.cells_written += k;
					}
					// Step 6: count the cells of the new rows
					for (const uint32_t x : new_) {
						lay_.for_each_cell(x, [&](uint32_t t) { indeg_[t]++; });
					}
					// Step 7: move the virtual initial edge (before the seeds are computed)
					indeg_[init_]--;
					init_ = x0;
					indeg_[x0]++;
				} catch (...) {
					// The handler only stores the exception: a throw from inside it faulted in the MSVC runtime
					// under clang with AddressSanitizer
					error = std::current_exception();
				}
				if (error) {
					// Old rows and counters are untouched before step 6, which cannot throw: free the new records
					for (const uint32_t x : new_) {
						delete_record(x, MinimalDfa::none);
					}
					pats_.pop_back();
					std::rethrow_exception(error);
				}
				st.live_region = new_.size();
				st.created = new_.size();
				st.candidates = cand_.size();
				garbage(st);  // step 8
				finish(st);
				return st;
			}

			// ------------------------------------------------------------ REMOVE
			UpdateStats remove(uint32_t pid) override {
				++op_;
				UpdateStats st;
				st.is_add = false;
				Pattern& P = pats_.at(pid);
				const std::vector<uint32_t> lambda = P.live;  // Λ = LiveSet[i]
				st.live_region = lambda.size();
				for (const uint32_t x : lambda) {
					lam_op_[x] = op_;
				}
				created_.clear();
				// Step 2: the record of each projected key, existing or created
				for (const uint32_t x : lambda) {
					kbuf_.clear();
					uint64_t h = key_seed;
					for (const KeyEntry& e : key_[x]) {
						if (e.pid != pid) {
							kbuf_.push_back(KeyEntry{e.pid, e.comp, 0});
							h = key_step(h, e.pid, e.comp);
						}
					}
					uint32_t y = index_find(kbuf_.data(), kbuf_.size(), h);
					if (y == no_record) {
						y = new_record(std::vector<KeyEntry>(kbuf_), h);
						created_.push_back(y);
						row_op_[y] = 0;
					} else if (created_op_[y] != op_) {
						st.merged++;
					}
					proj_[x] = y;
				}
				// Step 3: rows of created records, projected
				sbuf_.resize(k_);
				rbuf_.resize(k_);
				for (const uint32_t x : lambda) {
					const uint32_t y = proj_[x];
					if (created_op_[y] != op_ || row_op_[y] == op_) {
						continue;
					}
					lay_.read_row(x, rep_, k_, sbuf_.data());
					st.old_rows_read++;
					for (uint32_t c = 0; c < k_; c++) {
						const uint32_t z = sbuf_[c];
						rbuf_[c] = (lam_op_[z] == op_) ? proj_[z] : z;
					}
					lay_.write_row(y, rbuf_.data(), k_);
					row_op_[y] = op_;
					st.cells_written += k_;
				}
				// Step 4: counters
				for (const uint32_t x : lambda) {
					lay_.for_each_cell(x, [&](uint32_t t) { indeg_[t]--; });
				}
				for (const uint32_t y : created_) {
					lay_.for_each_cell(y, [&](uint32_t t) { indeg_[t]++; });
				}
				if (lam_op_[init_] == op_) {
					init_ = proj_[init_];
					indeg_[init_]++;
				}
				// Step 5: delete Λ; discard LiveSet[i] and A; keep the classes
				for (const uint32_t x : lambda) {
					delete_record(x, pid);
				}
				P.live.clear();
				P.live.shrink_to_fit();
				P.dfa = MinimalDfa();
				P.active = false;
				st.created = created_.size();
				st.deleted = lambda.size();
				finish(st);
				return st;
			}

			uint32_t final_state(const uint8_t* text, size_t length) const override {
				const uint32_t sink = sink_ == no_record ? Layout::invalid_cell : lay_.encode(sink_);
				return lay_.decode(lay_.walk(text, text + length, lay_.encode(init_), sink));
			}
			size_t state_count() const override { return records_; }
			std::vector<uint32_t> records() const override {
				std::vector<uint32_t> out;
				for (uint32_t r = 0; r < key_.size(); r++) {
					if (alive_[r]) {
						out.push_back(r);
					}
				}
				return out;
			}
			uint32_t transition(uint32_t rid, uint8_t byte) const override { return lay_.target(rid, byte); }
			size_t class_count() const override { return k_; }

			DynamicMemory memory() const override {
				DynamicMemory m;
				m.table_bytes = lay_.table_bytes();
				if constexpr (std::is_same_v<Layout, SlotTaggedLayout>) {
					m.table_live_bytes = lay_.table_live_bytes();
				} else {
					m.table_live_bytes = lay_.table_live_bytes(records_);
				}
				for (size_t r = 0; r < key_.size(); r++) {
					m.key_bytes += sizeof(std::vector<KeyEntry>) + key_[r].capacity() * sizeof(KeyEntry);
					m.output_bytes += sizeof(std::vector<Id>) + out_[r].capacity() * sizeof(Id);
				}
				m.index_bytes = index_.capacity() * sizeof(uint32_t);
				for (const Pattern& p : pats_) {
					m.live_set_bytes += sizeof(std::vector<uint32_t>) + p.live.capacity() * sizeof(uint32_t);
					m.pattern_bytes += sizeof(Pattern) + p.dfa.delta.capacity() * sizeof(uint32_t) + p.dfa.accepting.capacity();
				}
				m.counter_bytes = indeg_.capacity() * sizeof(uint32_t);
				m.other_bytes = hash_.capacity() * sizeof(uint64_t) + alive_.capacity() +
				                (created_op_.capacity() + cand_op_.capacity() + mark_op_.capacity() + lam_op_.capacity() +
				                 row_op_.capacity()) * sizeof(uint64_t) +
				                (proj_.capacity() + cin_.capacity() + free_rids_.capacity()) * sizeof(uint32_t) +
				                lay_.other_bytes();
				return m;
			}

			DynamicSnapshot<Id> snapshot() const override {
				DynamicSnapshot<Id> s;
				std::vector<uint32_t> dense(key_.size(), no_record);
				std::vector<uint32_t> order;
				for (uint32_t r = 0; r < key_.size(); r++) {
					if (alive_[r]) {
						dense[r] = static_cast<uint32_t>(order.size());
						order.push_back(r);
					}
				}
				s.states = order.size();
				s.initial = dense[init_];
				s.next.resize(order.size() * 256);
				s.outputs.resize(order.size());
				for (size_t i = 0; i < order.size(); i++) {
					for (size_t b = 0; b < 256; b++) {
						s.next[i * 256 + b] = dense[lay_.target(order[i], static_cast<uint8_t>(b))];
					}
					s.outputs[i] = out_[order[i]];
				}
				return s;
			}

			std::string check_invariants(bool deep) const override {
				std::string bad;
				const auto fail = [&](const std::string& what) {
					if (bad.size() < 2000) {
						bad += what + "; ";
					}
				};
				// (I4) counters, recomputed
				std::vector<uint32_t> count(key_.size(), 0);
				size_t alive_count = 0;
				for (uint32_t r = 0; r < key_.size(); r++) {
					if (!alive_[r]) {
						continue;
					}
					alive_count++;
					lay_.for_each_cell(r, [&](uint32_t t) {
						if (t >= alive_.size() || !alive_[t]) {
							fail("cell to a deleted record");
						} else {
							count[t]++;
						}
					});
				}
				if (!alive_[init_]) {
					fail("initial record deleted");
					return bad;
				}
				count[init_]++;
				for (uint32_t r = 0; r < key_.size(); r++) {
					if (alive_[r] && count[r] != indeg_[r]) {
						fail("counter of record " + std::to_string(r) + " is " + std::to_string(indeg_[r]) +
						     ", recomputed " + std::to_string(count[r]));
					}
				}
				if (alive_count != records_) {
					fail("record count");
				}
				// (I3) index and live sets
				size_t widths = 0;
				for (uint32_t r = 0; r < key_.size(); r++) {
					if (!alive_[r]) {
						continue;
					}
					const auto& K = key_[r];
					uint64_t h = key_seed;
					for (size_t j = 0; j < K.size(); j++) {
						if (j > 0 && K[j - 1].pid >= K[j].pid) {
							fail("key not sorted");
						}
						const Pattern& P = pats_[K[j].pid];
						if (!P.active) {
							fail("key holds a removed pattern");
							continue;
						}
						if (K[j].comp == P.dfa.dead || K[j].comp >= P.dfa.state_count) {
							fail("key holds a dead or bad component");
						}
						if (K[j].pos >= P.live.size() || P.live[K[j].pos] != r) {
							fail("live set position wrong");
						}
						h = key_step(h, K[j].pid, K[j].comp);
					}
					widths += K.size();
					if (h != hash_[r]) {
						fail("stored hash wrong");
					}
					if (index_find(K.data(), K.size(), h) != r) {
						fail("index does not find the record by its key");
					}
					std::vector<Id> o;
					for (const KeyEntry& e : K) {
						if (pats_[e.pid].dfa.accepting[e.comp]) {
							o.push_back(pats_[e.pid].id);
						}
					}
					std::sort(o.begin(), o.end());
					if (o != out_[r]) {
						fail("output differs from the key");
					}
				}
				size_t live_entries = 0;
				for (const Pattern& P : pats_) {
					if (P.active) {
						live_entries += P.live.size();
					}
				}
				if (live_entries != widths) {
					fail("live sets and keys disagree in size");
				}
				if (index_count_ != records_) {
					fail("index size");
				}
				// (I1) every record reachable
				std::vector<uint8_t> seen(key_.size(), 0);
				std::vector<uint32_t> stack{init_};
				seen[init_] = 1;
				size_t reached = 0;
				while (!stack.empty()) {
					const uint32_t r = stack.back();
					stack.pop_back();
					reached++;
					lay_.for_each_cell(r, [&](uint32_t t) {
						if (t < seen.size() && !seen[t]) {
							seen[t] = 1;
							stack.push_back(t);
						}
					});
				}
				if (reached != records_) {
					fail("only " + std::to_string(reached) + " of " + std::to_string(records_) + " records reachable");
				}
				// the initial record has the key of the initial tuple
				{
					std::vector<std::pair<uint32_t, uint32_t>> want;
					for (uint32_t p = 0; p < pats_.size(); p++) {
						if (pats_[p].active && pats_[p].dfa.initial != pats_[p].dfa.dead) {
							want.emplace_back(p, pats_[p].dfa.initial);
						}
					}
					std::vector<std::pair<uint32_t, uint32_t>> got;
					for (const KeyEntry& e : key_[init_]) {
						got.emplace_back(e.pid, e.comp);
					}
					if (want != got) {
						fail("initial record has the wrong key");
					}
				}
				if constexpr (std::is_same_v<Layout, SlotTaggedLayout>) {
					const std::string s = lay_.check(alive_);
					if (!s.empty()) {
						fail(s);
					}
				}
				// deep: every transition agrees with the stored automata
				if (deep) {
					std::vector<std::pair<uint32_t, uint32_t>> want;
					for (uint32_t r = 0; r < key_.size() && bad.size() < 2000; r++) {
						if (!alive_[r]) {
							continue;
						}
						for (size_t b = 0; b < 256; b++) {
							want.clear();
							for (const KeyEntry& e : key_[r]) {
								const MinimalDfa& d = pats_[e.pid].dfa;
								const uint32_t c2 = d.next(e.comp, static_cast<uint8_t>(b));
								if (c2 != d.dead) {
									want.emplace_back(e.pid, c2);
								}
							}
							const uint32_t y = lay_.target(r, static_cast<uint8_t>(b));
							bool same = y < alive_.size() && alive_[y] && key_[y].size() == want.size();
							for (size_t j = 0; same && j < want.size(); j++) {
								same = key_[y][j].pid == want[j].first && key_[y][j].comp == want[j].second;
							}
							if (!same) {
								fail("transition of record " + std::to_string(r) + " on byte " + std::to_string(b) +
								     " disagrees with the automata");
								break;
							}
						}
					}
				}
				return bad;
			}

		private:
			// ------------------------------------------------------------ REFINE (classes)
			void refine(const MinimalDfa& A, UpdateStats& st) {
				// New partition: the classes of kappa intersected with the classes of A. The child holding the
				// smallest byte keeps its parent's number (so rep of an old class does not change); other
				// children get new numbers.
				ClassMap ncls{};
				ClassMap nrep{};
				ClassMap parent{};
				std::array<uint16_t, 256> first_ac{};
				std::array<uint8_t, 256> seen{};
				struct Extra {
					uint8_t oc;
					uint16_t ac;
					uint8_t id;
				};
				std::vector<Extra> extras;
				uint32_t nk = k_;
				for (size_t b = 0; b < 256; b++) {
					const uint8_t oc = cls_[b];
					const uint16_t ac = A.byte_class[b];
					if (!seen[oc]) {
						seen[oc] = 1;
						first_ac[oc] = ac;
						ncls[b] = oc;
						nrep[oc] = static_cast<uint8_t>(b);
						parent[oc] = oc;
						continue;
					}
					if (ac == first_ac[oc]) {
						ncls[b] = oc;
						continue;
					}
					uint32_t id = no_record;
					for (const Extra& e : extras) {
						if (e.oc == oc && e.ac == ac) {
							id = e.id;
							break;
						}
					}
					if (id == no_record) {
						id = nk++;
						extras.push_back(Extra{oc, ac, static_cast<uint8_t>(id)});
						nrep[id] = static_cast<uint8_t>(b);
						parent[id] = oc;
					}
					ncls[b] = static_cast<uint8_t>(id);
				}
				if (nk == k_) {
					return;
				}
				st.class_splits = 1;
				const auto inc = [this](uint32_t rid, uint32_t n) { indeg_[rid] += n; };
				if constexpr (std::is_same_v<Layout, SlotTaggedLayout>) {
					lay_.refine(ncls, nk, nrep, inc, st);
				} else {
					const auto for_each_live = [this](auto&& f) {
						for (uint32_t r = 0; r < key_.size(); r++) {
							if (alive_[r]) {
								f(r);
							}
						}
					};
					lay_.refine(ncls, nk, parent, for_each_live, inc, st);
				}
				st.cells_written += st.refine_cells_written;
				cls_ = ncls;
				rep_ = nrep;
				k_ = nk;
			}

			// ------------------------------------------------------------ GARBAGE (least fixpoint)
			void garbage(UpdateStats& st) {
				// Step 1: cells from C into C
				for (const uint32_t x : cand_) {
					cin_[x] = 0;
				}
				for (const uint32_t y : cand_) {
					st.garbage_row_reads++;
					lay_.for_each_cell(y, [&](uint32_t z) {
						if (cand_op_[z] == op_) {
							cin_[z]++;
						}
					});
				}
				// Steps 2 and 3: seeds from the counters, then marking inside C
				stack_.clear();
				for (const uint32_t x : cand_) {
					if (indeg_[x] > cin_[x]) {  // indeg - cin > 0: an entry or the virtual edge
						mark_op_[x] = op_;
						stack_.push_back(x);
					}
				}
				while (!stack_.empty()) {
					const uint32_t y = stack_.back();
					stack_.pop_back();
					st.garbage_row_reads++;
					lay_.for_each_cell(y, [&](uint32_t z) {
						if (cand_op_[z] == op_ && mark_op_[z] != op_) {
							mark_op_[z] = op_;
							stack_.push_back(z);
						}
					});
				}
				// Step 4: G' = C minus M
				gone_.clear();
				for (const uint32_t x : cand_) {
					if (mark_op_[x] != op_) {
						gone_.push_back(x);
					}
				}
				for (const uint32_t y : gone_) {
					st.garbage_row_reads++;
					lay_.for_each_cell(y, [&](uint32_t t) { indeg_[t]--; });
				}
				for (const uint32_t y : gone_) {
					delete_record(y, MinimalDfa::none);
				}
				st.deleted = gone_.size();
			}

			void finish(UpdateStats& st) const {
				st.states = records_;
				st.classes = k_;
				st.table_bytes = lay_.table_bytes();
				st.slots_in_use = lay_.slots_in_use();
			}

			// ------------------------------------------------------------ records
			uint32_t alloc_rid() {
				if (!free_rids_.empty()) {
					const uint32_t r = free_rids_.back();
					free_rids_.pop_back();
					return r;
				}
				const size_t r = key_.size();
				if (r >= no_record - 1) {
					throw std::length_error("DynamicSet: too many records");
				}
				lay_.reserve_records(r + 1);
				key_.emplace_back();
				hash_.push_back(0);
				indeg_.push_back(0);
				alive_.push_back(0);
				created_op_.push_back(0);
				cand_op_.push_back(0);
				mark_op_.push_back(0);
				lam_op_.push_back(0);
				row_op_.push_back(0);
				proj_.push_back(0);
				cin_.push_back(0);
				out_.emplace_back();
				return static_cast<uint32_t>(r);
			}

			uint32_t new_record(std::vector<KeyEntry>&& key, uint64_t h) {
				const uint32_t r = alloc_rid();
				std::exception_ptr error;
				try {
					lay_.alloc_row(r);
				} catch (...) {
					error = std::current_exception();
				}
				if (error) {
					free_rids_.push_back(r);
					std::rethrow_exception(error);
				}
				key_[r] = std::move(key);
				hash_[r] = h;
				indeg_[r] = 0;
				alive_[r] = 1;
				created_op_[r] = op_;
				std::vector<Id>& o = out_[r];
				o.clear();
				for (KeyEntry& e : key_[r]) {
					Pattern& P = pats_[e.pid];
					e.pos = static_cast<uint32_t>(P.live.size());
					P.live.push_back(r);
					if (P.dfa.accepting[e.comp]) {
						o.push_back(P.id);
					}
				}
				std::sort(o.begin(), o.end());
				if (key_[r].empty()) {
					sink_ = r;
				}
				index_insert(r);
				records_++;
				return r;
			}

			// Deletes a record. skip_pid: a pattern whose live set is discarded as a whole by the caller.
			void delete_record(uint32_t r, uint32_t skip_pid) {
				index_erase(r);
				for (const KeyEntry& e : key_[r]) {
					if (e.pid == skip_pid) {
						continue;
					}
					std::vector<uint32_t>& L = pats_[e.pid].live;
					const uint32_t moved = L.back();
					L[e.pos] = moved;
					L.pop_back();
					if (moved != r) {
						auto& mk = key_[moved];
						const auto it = std::lower_bound(mk.begin(), mk.end(), e.pid,
						                                 [](const KeyEntry& a, uint32_t p) { return a.pid < p; });
						it->pos = e.pos;
					}
				}
				lay_.free_row(r);
				if (sink_ == r) {
					sink_ = no_record;
				}
				key_[r] = std::vector<KeyEntry>();
				out_[r] = std::vector<Id>();
				alive_[r] = 0;
				free_rids_.push_back(r);
				records_--;
			}

			// ------------------------------------------------------------ index: key -> record
			bool key_equal(uint32_t r, const KeyEntry* k, size_t w) const {
				const auto& K = key_[r];
				if (K.size() != w) {
					return false;
				}
				for (size_t j = 0; j < w; j++) {
					if (K[j].pid != k[j].pid || K[j].comp != k[j].comp) {
						return false;
					}
				}
				return true;
			}
			uint32_t index_find(const KeyEntry* k, size_t w, uint64_t h) const {
				if (index_.empty()) {
					return no_record;
				}
				for (size_t i = h & index_mask_;; i = (i + 1) & index_mask_) {
					const uint32_t r = index_[i];
					if (r == no_record) {
						return no_record;
					}
					if (hash_[r] == h && key_equal(r, k, w)) {
						return r;
					}
				}
			}
			void index_insert(uint32_t r) {
				if ((index_count_ + 1) * 2 > index_.size()) {
					const size_t size = index_.empty() ? 1024 : index_.size() * 2;
					std::vector<uint32_t> old;
					old.swap(index_);
					index_.assign(size, no_record);
					index_mask_ = size - 1;
					for (const uint32_t x : old) {
						if (x != no_record) {
							place(x);
						}
					}
				}
				place(r);
				index_count_++;
			}
			void place(uint32_t r) {
				for (size_t i = hash_[r] & index_mask_;; i = (i + 1) & index_mask_) {
					if (index_[i] == no_record) {
						index_[i] = r;
						return;
					}
				}
			}
			void index_erase(uint32_t r) {
				size_t i = hash_[r] & index_mask_;
				while (index_[i] != r) {
					i = (i + 1) & index_mask_;
				}
				// backward shift deletion
				size_t j = i;
				while (true) {
					j = (j + 1) & index_mask_;
					const uint32_t x = index_[j];
					if (x == no_record) {
						break;
					}
					const size_t home = hash_[x] & index_mask_;
					if (((j - home) & index_mask_) >= ((j - i) & index_mask_)) {
						index_[i] = x;
						i = j;
					}
				}
				index_[i] = no_record;
				index_count_--;
			}

			Layout lay_;
			size_t max_states_;
			std::vector<Pattern> pats_;
			// per record
			std::vector<std::vector<KeyEntry>> key_;
			std::vector<uint64_t> hash_;
			std::vector<uint32_t> indeg_;
			std::vector<uint8_t> alive_;
			std::vector<uint64_t> created_op_;
			std::vector<uint64_t> cand_op_;
			std::vector<uint64_t> mark_op_;
			std::vector<uint64_t> lam_op_;
			std::vector<uint64_t> row_op_;
			std::vector<uint32_t> proj_;
			std::vector<uint32_t> cin_;
			std::vector<uint32_t> free_rids_;
			size_t records_ = 0;
			uint32_t init_ = 0;
			uint32_t sink_ = no_record;  // the record with the empty key, if it exists
			// partition
			ClassMap cls_{};
			ClassMap rep_{};
			uint32_t k_ = 1;
			// index
			std::vector<uint32_t> index_;
			size_t index_mask_ = 0;
			size_t index_count_ = 0;
			// scratch of one update
			uint64_t op_ = 0;
			PairTable pairs_;
			std::vector<uint32_t> cand_, new_, created_, stack_, gone_, cmA_, sbuf_, rbuf_;
			std::vector<Item> queue_;
			std::vector<KeyEntry> kbuf_;
		};
	}  // namespace detail

	// ------------------------------------------------------------------ DynamicSet

	template <typename Id>
	DynamicSet<Id>::DynamicSet(const DynamicOptions& options) : options_(options) {
		switch (options.layout) {
			case DynamicLayout::slot_tagged:
				engine_ = std::make_unique<detail::DynamicCore<Id, detail::SlotTaggedLayout>>(options, false);
				break;
			case DynamicLayout::dense_spare:
				engine_ = std::make_unique<detail::DynamicCore<Id, detail::DenseSpareLayout>>(options, false);
				break;
			case DynamicLayout::full_bytes:
				engine_ = std::make_unique<detail::DynamicCore<Id, detail::FullBytesLayout>>(options, true);
				break;
			default:
				throw std::invalid_argument("DynamicSet: unknown layout");
		}
	}

	template <typename Id>
	template <typename Iterable>
	UpdateStats DynamicSet<Id>::add_regex(const Iterable& pattern, const Id& id) {
		if (contains(id)) {
			throw std::invalid_argument("DynamicSet: id already present");
		}
		MinimalDfa dfa = compile_regex_dfa(pattern);
		const UpdateStats st = engine_->add(next_pid_, id, std::move(dfa));
		pid_of_.emplace(id, next_pid_++);
		return st;
	}

	template <typename Id>
	UpdateStats DynamicSet<Id>::add_dfa(const MinimalDfa& dfa, const Id& id) {
		if (contains(id)) {
			throw std::invalid_argument("DynamicSet: id already present");
		}
		MinimalDfa d;
		if (options_.minimize_input) {
			d = minimize_dfa(dfa);
		} else {
			validate_dfa(dfa);
			d = dfa;
			d.dead = detail::find_dead_state(d);
		}
		const UpdateStats st = engine_->add(next_pid_, id, std::move(d));
		pid_of_.emplace(id, next_pid_++);
		return st;
	}

	template <typename Id>
	UpdateStats DynamicSet<Id>::remove(const Id& id) {
		const auto it = pid_of_.find(id);
		if (it == pid_of_.end()) {
			throw std::invalid_argument("DynamicSet: id not present");
		}
		const uint32_t pid = it->second;
		pid_of_.erase(it);
		return engine_->remove(pid);
	}

	template <typename Id>
	size_t DynamicSet<Id>::state_count() const {
		return engine_->state_count();
	}
	template <typename Id>
	size_t DynamicSet<Id>::class_count() const {
		return engine_->class_count();
	}
	template <typename Id>
	DynamicMemory DynamicSet<Id>::memory() const {
		return engine_->memory();
	}
	template <typename Id>
	DynamicSnapshot<Id> DynamicSet<Id>::snapshot() const {
		return engine_->snapshot();
	}
	template <typename Id>
	std::string DynamicSet<Id>::check_invariants(bool deep) const {
		return engine_->check_invariants(deep);
	}
	template <typename Id>
	uint32_t DynamicSet<Id>::final_state(const uint8_t* text, size_t length) const {
		return engine_->final_state(text, length);
	}
	template <typename Id>
	const std::vector<Id>& DynamicSet<Id>::outputs_of(uint32_t state) const {
		return engine_->outputs_of(state);
	}

	template <typename Id>
	std::vector<uint32_t> DynamicSet<Id>::records() const {
		return engine_->records();
	}
	template <typename Id>
	uint32_t DynamicSet<Id>::transition(uint32_t state, uint8_t byte) const {
		return engine_->transition(state, byte);
	}

	template <typename Id>
	template <typename Iterable, typename Visit>
	decltype(auto) DynamicSet<Id>::with_ids(const Iterable& text, Visit&& visit) const {
		static_assert(!std::is_array_v<Iterable>,
		              "Pass a std::string or std::string_view, not a C array: its final '\\0' would be matched.");
		uint32_t s = 0;
		if constexpr (detail::is_contiguous_text<Iterable>::value) {
			const auto* data = std::data(text);
			static_assert(sizeof(*data) == 1, "DynamicSet matches bytes");
			s = engine_->final_state(reinterpret_cast<const uint8_t*>(data), std::size(text));
		} else {
			const std::string copy(std::cbegin(text), std::cend(text));
			s = engine_->final_state(reinterpret_cast<const uint8_t*>(copy.data()), copy.size());
		}
		return visit(engine_->outputs_of(s));
	}

	template <typename Id>
	template <typename Iterable>
	std::vector<Id> DynamicSet<Id>::match(const Iterable& text) const {
		return with_ids(text, [](const std::vector<Id>& ids) { return ids; });
	}

	template <typename Id>
	template <typename Iterable, typename Callback>
	size_t DynamicSet<Id>::for_each_match(const Iterable& text, Callback&& callback) const {
		return with_ids(text, [&callback](const std::vector<Id>& ids) {
			size_t calls = 0;
			for (const Id& id : ids) {
				calls++;
				if constexpr (std::is_same_v<std::invoke_result_t<Callback&, const Id&>, bool>) {
					if (!callback(id)) {
						break;
					}
				} else {
					callback(id);
				}
			}
			return calls;
		});
	}

	template <typename Id>
	template <typename Iterable, typename Container>
	size_t DynamicSet<Id>::match_into(const Iterable& text, Container& out) const {
		return with_ids(text, [&out](const std::vector<Id>& ids) {
			out.clear();
			out.insert(out.end(), ids.begin(), ids.end());
			return ids.size();
		});
	}

	template <typename Id>
	template <typename Iterable>
	bool DynamicSet<Id>::matches_any(const Iterable& text) const {
		return with_ids(text, [](const std::vector<Id>& ids) { return !ids.empty(); });
	}

}  // namespace matcher

#undef MATCHER_DYNAMIC_CHECK
