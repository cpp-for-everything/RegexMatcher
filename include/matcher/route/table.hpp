#pragma once

// The flat table (nodes, edges, exact-match slots and one label arena) and its build.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <span>
#include <string_view>
#include <vector>

#include <matcher/route/config.hpp>
#include <matcher/route/pattern.hpp>
#include <matcher/route/detail/words.hpp>

namespace matcher::route
{

	// ========================================================================
	// The flat table
	// ========================================================================

	struct Edge
	{
		std::uint64_t word = 0;      // the first 8 bytes of the label, zero-padded
		std::uint32_t child = kNone;  // kNone marks an empty slot of a hashed node
		std::uint32_t off = 0;       // the label in the arena
		std::uint32_t len = 0;
	};

	struct Node
	{
		std::uint32_t edges = 0;  // first edge (linear) or first slot (hashed)
		std::uint32_t count = 0;  // edges (linear) or slots, a power of two (hashed)
		std::uint8_t hashed = 0;
		std::uint8_t branches = 0;  // how many kinds of child: a lookup backtracks only if > 1
		std::uint16_t pad = 0;
		std::uint32_t u64 = kNone;
		std::uint32_t i64 = kNone;
		std::uint32_t param = kNone;
		std::uint32_t rest = kNone;
		std::uint32_t route = kNone;
	};

	struct LiteralSlot
	{
		std::uint64_t hash = 0;
		std::uint32_t off = 0;
		std::uint32_t len = 0;
		std::uint32_t route = kNone;  // kNone marks an empty slot
		std::uint32_t method = 0;
	};

	constexpr std::array<std::uint32_t, kMethodSlots> no_roots() noexcept
	{
		std::array<std::uint32_t, kMethodSlots> r{};
		r.fill(kNone);
		return r;
	}

	struct TableView
	{
		std::span<const Node> nodes;
		std::span<const Edge> edges;
		std::span<const LiteralSlot> literals;  // empty, or a power of two
		std::string_view arena;
		std::array<std::uint32_t, kMethodSlots> roots = no_roots();  // kNone: no trie for the method
		std::uint16_t methods = 0;                        // bit m: some route has method m
		std::uint16_t literal_methods = 0;                // bit m: some literal route has method m
	};

	// ========================================================================
	// Building a table
	// ========================================================================

	enum class BuildError : std::uint8_t
	{
		None,
		BadPattern,
		BadMethod,
		Duplicate,
		TooLarge
	};

	struct RouteSpec
	{
		unsigned method = 0;
		std::string_view pattern;
		std::uint32_t route = 0;  // the value the lookup returns for this route
	};

	// Every array of a table starts on a 64-byte boundary, in a table built at run time
	// (this allocator) as in one built while compiling (StaticTable), so that the two lay out
	// their nodes, edges and slots against cache lines alike. Constant evaluation takes
	// std::allocator, the only allocator it accepts; the result is copied into StaticTable.
	inline constexpr std::size_t kTableAlign = 64;

	// A RuntimeTable's block starts on a page. A StaticTable declared alignas(kPageAlign) does
	// too, and then every element of either table lies at the same offset into a page.
	inline constexpr std::size_t kPageAlign = 4096;

	// The lengths of a table's four arrays.
	struct StaticSizes
	{
		std::size_t nodes = 0;
		std::size_t edges = 0;
		std::size_t literals = 0;
		std::size_t arena = 0;
	};

	// Where a table's four arrays lie in one block (a StaticTable's members, a RuntimeTable's
	// block): nodes, edges, exact-match slots and the arena, in that order, each at the first
	// multiple of kTableAlign after the one before; the arena has one byte more than its labels.
	struct TableLayout
	{
		std::size_t nodes = 0;  // offsets into the block
		std::size_t edges = 0;
		std::size_t literals = 0;
		std::size_t arena = 0;
		std::size_t bytes = 0;  // the block's size
	};

	namespace detail
	{
		constexpr std::size_t align_up(std::size_t n, std::size_t a) noexcept
		{
			return (n + a - 1) / a * a;
		}

		// The bytes an array member of StaticTable takes. An empty std::array takes some too, as
		// many as the standard library gives it, and the next member comes after them.
		template <class T>
		constexpr std::size_t member_bytes(std::size_t n) noexcept
		{
			return n != 0 ? n * sizeof(T) : sizeof(std::array<T, 0>);
		}
	}  // namespace detail

	constexpr TableLayout table_layout(const StaticSizes& s) noexcept
	{
		TableLayout l;
		l.edges = detail::align_up(l.nodes + detail::member_bytes<Node>(s.nodes), kTableAlign);
		l.literals = detail::align_up(l.edges + detail::member_bytes<Edge>(s.edges), kTableAlign);
		l.arena = detail::align_up(l.literals + detail::member_bytes<LiteralSlot>(s.literals), kTableAlign);
		l.bytes = l.arena + s.arena + 1;
		return l;
	}

	template <class T>
	struct TableAllocator
	{
		using value_type = T;

		constexpr TableAllocator() noexcept = default;
		template <class U>
		constexpr TableAllocator(const TableAllocator<U>&) noexcept
		{
		}

		constexpr T* allocate(std::size_t n)
		{
			if consteval
			{
				return std::allocator<T>{}.allocate(n);
			}
			else
			{
				return static_cast<T*>(::operator new(n * sizeof(T), std::align_val_t{kTableAlign}));
			}
		}
		constexpr void deallocate(T* p, std::size_t n) noexcept
		{
			if consteval
			{
				std::allocator<T>{}.deallocate(p, n);
			}
			else
			{
				::operator delete(p, std::align_val_t{kTableAlign});  // unsized: portable without -fsized-deallocation
			}
		}
		template <class U>
		friend constexpr bool operator==(const TableAllocator&, const TableAllocator<U>&) noexcept
		{
			return true;
		}
	};

	template <class T>
	using TableVector = std::vector<T, TableAllocator<T>>;

	// The flat arrays as vectors: what a run-time table owns, and what a compile-time build
	// fills before copying into fixed-size arrays.
	struct Built
	{
		TableVector<Node> nodes;
		TableVector<Edge> edges;
		TableVector<LiteralSlot> literals;
		TableVector<char> arena;
		std::array<std::uint32_t, kMethodSlots> roots = no_roots();
		std::uint16_t methods = 0;
		std::uint16_t literal_methods = 0;
		BuildError error = BuildError::None;
		std::uint32_t error_route = 0;   // the route (index into the specs) that failed
		std::uint32_t error_other = 0;   // with Duplicate: the route it duplicates

		constexpr TableView view() const noexcept
		{
			return {nodes, edges, literals, std::string_view(arena.data(), arena.size()), roots, methods,
			        literal_methods};
		}
	};

	namespace detail
	{
		// One route while a table is built: its method, value and index among the specs, and its
		// segments as a run [first, first + n) of the build's one array of segments. Only the
		// segments a pattern has are stored (a Pattern holds room for 32), and the build sorts and
		// splits indices of entries, never the entries themselves, so a route costs a few dozen
		// bytes of bookkeeping instead of a Pattern's 784.
		struct Entry
		{
			unsigned method;
			std::uint32_t route;
			std::uint32_t spec;
			std::uint32_t first;  // the entry's first segment in Build::segs
			std::uint8_t n;       // its number of segments; "/" has none
			std::uint8_t params;  // its typed, plain and catch-all segments
		};

		// The order a trie is built in: by method, then segment by segment (kind, type, text),
		// a route that ends before another first.
		constexpr int compare_seg(const Segment& a, const Segment& b) noexcept
		{
			if (a.kind != b.kind)
			{
				return a.kind < b.kind ? -1 : 1;
			}
			if (a.kind == SegKind::Typed && a.type != b.type)
			{
				return a.type < b.type ? -1 : 1;
			}
			if (a.kind == SegKind::Literal)
			{
				return a.text < b.text ? -1 : (a.text == b.text ? 0 : 1);
			}
			return 0;  // parameters match the same segments whatever their names
		}

		// The entries of a build and their segments, and the comparisons over them.
		struct Build
		{
			std::vector<Entry> entries;
			std::vector<Segment> segs;

			constexpr const Segment& seg(std::uint32_t e, std::size_t d) const noexcept { return segs[entries[e].first + d]; }

			constexpr bool less(std::uint32_t x, std::uint32_t y) const noexcept
			{
				const Entry& a = entries[x];
				const Entry& b = entries[y];
				if (a.method != b.method)
				{
					return a.method < b.method;
				}
				const std::size_t n = std::min(a.n, b.n);
				for (std::size_t i = 0; i < n; ++i)
				{
					const int c = compare_seg(segs[a.first + i], segs[b.first + i]);
					if (c != 0)
					{
						return c < 0;
					}
				}
				if (a.n != b.n)
				{
					return a.n < b.n;
				}
				return a.spec < b.spec;
			}

			constexpr bool same_paths(std::uint32_t x, std::uint32_t y) const noexcept
			{
				const Entry& a = entries[x];
				const Entry& b = entries[y];
				if (a.method != b.method || a.n != b.n)
				{
					return false;
				}
				for (std::size_t i = 0; i < a.n; ++i)
				{
					if (compare_seg(segs[a.first + i], segs[b.first + i]) != 0)
					{
						return false;
					}
				}
				return true;
			}
		};

		constexpr std::size_t pow2_at_least(std::size_t n) noexcept
		{
			std::size_t p = 1;
			while (p < n)
			{
				p <<= 1;
			}
			return p;
		}

		inline constexpr std::size_t kLinearEdges = 8;

		struct TrieBuilder
		{
			Built& out;
			const Build& build_;
			const std::vector<std::uint32_t>& order;  // the trie's entries, sorted
			// The literal groups of the nodes being built, one run per node on the path from the
			// root: a node pushes its groups, builds its children, and pops them, so the build
			// allocates for this stack as it grows, not once per node.
			std::vector<std::pair<std::size_t, std::size_t>> groups{};

			constexpr std::uint32_t label(std::string_view s)
			{
				const auto off = static_cast<std::uint32_t>(out.arena.size());
				out.arena.insert(out.arena.end(), s.begin(), s.end());
				return off;
			}

			constexpr const Segment& seg(std::size_t i, std::size_t depth) const noexcept
			{
				return build_.seg(order[i], depth);
			}

			// Builds the node for entries [lo, hi) of order, which share their first `depth` segments.
			constexpr std::uint32_t build(std::size_t lo, std::size_t hi, std::size_t depth)
			{
				const auto self = static_cast<std::uint32_t>(out.nodes.size());
				out.nodes.push_back({});
				std::uint32_t route = kNone;
				// Entries that end here come first in the sort order.
				while (lo < hi && build_.entries[order[lo]].n == depth)
				{
					route = build_.entries[order[lo]].route;
					++lo;
				}
				// Group the rest by their segment at `depth`.
				const std::size_t base = groups.size();
				std::array<std::pair<std::size_t, std::size_t>, 4> other{};  // u64, i64, param, rest
				for (auto& o : other)
				{
					o = {0, 0};
				}
				for (std::size_t i = lo; i < hi;)
				{
					std::size_t j = i + 1;
					while (j < hi && compare_seg(seg(i, depth), seg(j, depth)) == 0)
					{
						++j;
					}
					const Segment& s = seg(i, depth);
					switch (s.kind)
					{
						case SegKind::Literal: groups.push_back({i, j}); break;
						case SegKind::Typed: other[s.type == ParamType::U64 ? 0 : 1] = {i, j}; break;
						case SegKind::Param: other[2] = {i, j}; break;
						case SegKind::Rest: other[3] = {i, j}; break;
					}
					i = j;
				}
				const std::size_t count = groups.size() - base;
				const bool hashed = count > kLinearEdges;
				const std::size_t slots = hashed ? pow2_at_least(2 * count) : count;
				const auto first = static_cast<std::uint32_t>(out.edges.size());
				out.edges.resize(out.edges.size() + slots);
				std::uint8_t branches = count ? 1 : 0;
				for (const auto& o : other)
				{
					branches = static_cast<std::uint8_t>(branches + (o.second > o.first ? 1 : 0));
				}
				{
					Node& n = out.nodes[self];
					n.edges = first;
					n.count = static_cast<std::uint32_t>(slots);
					n.hashed = hashed ? 1 : 0;
					n.branches = branches;
					n.route = route;
				}
				for (std::size_t g = 0; g < count; ++g)
				{
					const auto [a, b] = groups[base + g];  // by value: a child's groups may reallocate
					const std::string_view text = seg(a, depth).text;
					Edge e;
					e.len = static_cast<std::uint32_t>(text.size());
					e.word = load_at(text, 0);
					e.off = label(text);
					e.child = build(a, b, depth + 1);
					if (!hashed)
					{
						out.edges[first + g] = e;
					}
					else
					{
						const std::size_t mask = slots - 1;
						std::size_t i = static_cast<std::size_t>(edge_hash(e.word, text)) & mask;
						while (out.edges[first + i].child != kNone)
						{
							i = (i + 1) & mask;
						}
						out.edges[first + i] = e;
					}
				}
				groups.resize(base);
				std::array<std::uint32_t, 4> kids{kNone, kNone, kNone, kNone};
				for (std::size_t k = 0; k < 4; ++k)
				{
					if (other[k].second > other[k].first)
					{
						kids[k] = build(other[k].first, other[k].second, depth + 1);
					}
				}
				Node& n = out.nodes[self];
				n.u64 = kids[0];
				n.i64 = kids[1];
				n.param = kids[2];
				n.rest = kids[3];
				return self;
			}
		};
	}  // namespace detail

	// Builds a table from routes. Every pattern must parse; two routes of one method that
	// match the same paths are an error (Built::error names them).
	constexpr Built build_table(std::span<const RouteSpec> specs)
	{
		Built out;
		out.roots.fill(kNone);
		detail::Build b;
		b.entries.reserve(specs.size());
		{
			// A pattern has at most as many segments as it has '/', so one allocation holds them all.
			std::size_t slashes = 0;
			for (const RouteSpec& s : specs)
			{
				for (const char c : s.pattern)
				{
					slashes += c == '/' ? 1 : 0;
				}
			}
			b.segs.reserve(slashes);
		}
		std::uint16_t param_methods = 0;  // bit m: some route of method m has a parameter
		for (std::size_t i = 0; i < specs.size(); ++i)
		{
			const RouteSpec& s = specs[i];
			if (s.method >= kMethodSlots)
			{
				out.error = BuildError::BadMethod;
				out.error_route = static_cast<std::uint32_t>(i);
				return out;
			}
			const Pattern p = parse_pattern(s.pattern);
			if (!p.ok())
			{
				out.error = BuildError::BadPattern;
				out.error_route = static_cast<std::uint32_t>(i);
				return out;
			}
			b.entries.push_back({s.method, s.route, static_cast<std::uint32_t>(i),
			                     static_cast<std::uint32_t>(b.segs.size()), p.n, p.params});
			b.segs.insert(b.segs.end(), p.segs.begin(), p.segs.begin() + p.n);
			out.methods = static_cast<std::uint16_t>(out.methods | (1u << s.method));
			if (!p.literal())
			{
				param_methods = static_cast<std::uint16_t>(param_methods | (1u << s.method));
			}
		}
		// A method whose routes are all literal is answered by the exact-match table alone. A
		// method that also has parameter routes keeps its literal routes in its trie: the walk
		// tries a literal child before a parameter, so it finds a fully literal route first, as
		// rule 5 wants, and the path is read once instead of hashed and then walked.
		std::vector<std::uint32_t> literal;
		std::vector<std::uint32_t> trie;
		{
			std::size_t nlit = 0;
			for (const auto& e : b.entries)
			{
				nlit += e.params == 0 && !(param_methods & (1u << e.method)) ? 1 : 0;
			}
			literal.reserve(nlit);
			trie.reserve(b.entries.size() - nlit);
		}
		for (std::uint32_t i = 0; i < b.entries.size(); ++i)
		{
			const auto& e = b.entries[i];
			(e.params == 0 && !(param_methods & (1u << e.method)) ? literal : trie).push_back(i);
		}
		const auto less = [&b](std::uint32_t x, std::uint32_t y) { return b.less(x, y); };
		for (auto* list : {&literal, &trie})
		{
			std::sort(list->begin(), list->end(), less);
			for (std::size_t i = 1; i < list->size(); ++i)
			{
				if (b.same_paths((*list)[i - 1], (*list)[i]))
				{
					out.error = BuildError::Duplicate;
					out.error_route = b.entries[(*list)[i]].spec;
					out.error_other = b.entries[(*list)[i - 1]].spec;
					return out;
				}
			}
		}
		// The literal table: every literal route by method and whole path.
		if (!literal.empty())
		{
			const std::size_t slots = detail::pow2_at_least(2 * literal.size());
			out.literals.resize(slots);
			for (const std::uint32_t k : literal)
			{
				const auto& e = b.entries[k];
				const std::string_view path = e.n == 0 ? std::string_view("/") : specs[e.spec].pattern;
				LiteralSlot s;
				s.hash = detail::path_hash(path, e.method);
				s.len = static_cast<std::uint32_t>(path.size());
				s.off = static_cast<std::uint32_t>(out.arena.size());
				out.arena.insert(out.arena.end(), path.begin(), path.end());
				s.route = e.route;
				s.method = e.method;
				std::size_t i = static_cast<std::size_t>(s.hash) & (slots - 1);
				while (out.literals[i].route != kNone)
				{
					i = (i + 1) & (slots - 1);
				}
				out.literals[i] = s;
				out.literal_methods = static_cast<std::uint16_t>(out.literal_methods | (1u << e.method));
			}
		}
		// One trie per method.
		detail::TrieBuilder t{out, b, trie};
		for (std::size_t lo = 0; lo < trie.size();)
		{
			std::size_t hi = lo;
			while (hi < trie.size() && b.entries[trie[hi]].method == b.entries[trie[lo]].method)
			{
				++hi;
			}
			out.roots[b.entries[trie[lo]].method] = t.build(lo, hi, 0);
			lo = hi;
		}
		if (out.nodes.size() >= kNone || out.edges.size() >= kNone || out.arena.size() >= kNone)
		{
			out.error = BuildError::TooLarge;
		}
		return out;
	}

}  // namespace matcher::route
