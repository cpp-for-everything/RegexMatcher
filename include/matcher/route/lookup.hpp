#pragma once

// The lookup: one request's method and path to a route and its captured values.

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <matcher/route/config.hpp>
#include <matcher/route/pattern.hpp>
#include <matcher/route/detail/words.hpp>
#include <matcher/route/table.hpp>

namespace matcher::route
{

	enum class Status : std::uint8_t
	{
		Found,
		MethodNotAllowed,
		NotFound
	};

	// A captured value: a view into the path the lookup was given. Trivial on purpose, so
	// that the captures of a lookup are never zeroed at run time; only the first `count`
	// of them are ever set or read.
	struct Capture
	{
		const char* ptr;
		std::size_t len;

		constexpr std::string_view view() const noexcept { return {ptr, len}; }
		constexpr operator std::string_view() const noexcept { return view(); }
		constexpr const char* data() const noexcept { return ptr; }
		constexpr std::size_t size() const noexcept { return len; }
		constexpr bool empty() const noexcept { return len == 0; }
		friend constexpr bool operator==(const Capture& a, std::string_view b) noexcept { return a.view() == b; }
	};

	struct Match
	{
		Status status = Status::NotFound;
		std::uint8_t count = 0;     // captured values
		std::uint16_t allowed = 0;  // with MethodNotAllowed: bit m set for every method that matches
		std::uint32_t route = kNone;
		std::array<Capture, kMaxParams> params;

		constexpr Match() noexcept
		{
			if consteval
			{
				params.fill(Capture{nullptr, 0});
			}
		}

		constexpr explicit operator bool() const noexcept { return status == Status::Found; }
		constexpr std::span<const Capture> values() const noexcept { return {params.data(), count}; }
	};

	namespace detail
	{
		MATCHER_ROUTE_INLINE constexpr std::uint32_t find_literal(const TableView& t, unsigned method,
		                                                            std::string_view path) noexcept
		{
			if (t.literals.empty() || !(t.literal_methods & (1u << method)))
			{
				return kNone;
			}
			const std::uint64_t h = path_hash(path, method);
			const std::size_t mask = t.literals.size() - 1;
			for (std::size_t i = static_cast<std::size_t>(h) & mask;; i = (i + 1) & mask)
			{
				const LiteralSlot& s = t.literals[i];
				if (s.route == kNone)
				{
					return kNone;
				}
				if (s.hash == h && s.method == method && s.len == path.size() &&
				    same_from(t.arena.data() + s.off, path.data(), s.len, 0))
				{
					return s.route;
				}
			}
		}

		MATCHER_ROUTE_INLINE constexpr std::uint32_t find_edge(const TableView& t, const Node& n, std::string_view seg,
		                                                         std::uint64_t word) noexcept
		{
			const auto matches = [&](const Edge& e) {
				return e.word == word && e.len == seg.size() &&
				       (seg.size() <= 8 || same_from(t.arena.data() + e.off, seg.data(), seg.size(), 8));
			};
			if (!n.hashed)
			{
				for (std::uint32_t i = n.edges, end = n.edges + n.count; i < end; ++i)
				{
					if (matches(t.edges[i]))
					{
						return t.edges[i].child;
					}
				}
				return kNone;
			}
			const std::size_t mask = n.count - 1;
			for (std::size_t i = static_cast<std::size_t>(edge_hash(word, seg)) & mask;; i = (i + 1) & mask)
			{
				const Edge& e = t.edges[n.edges + i];
				if (e.child == kNone)
				{
					return kNone;
				}
				if (matches(e))
				{
					return e.child;
				}
			}
		}

		// Whether the n bytes of a at ia are the n bytes of b at ib, both in range: 8 bytes at a
		// time, the last word masked to the bytes that remain.
		MATCHER_ROUTE_INLINE constexpr bool same_run(std::string_view a, std::size_t ia, std::string_view b,
		                                             std::size_t ib, std::size_t n) noexcept
		{
			for (std::size_t i = 0; i < n; i += 8)
			{
				if (((load_at(a, ia + i) ^ load_at(b, ib + i)) & mask_low(n - i)) != 0)
				{
					return false;
				}
			}
			return true;
		}

		struct Choice
		{
			std::uint32_t node;
			std::uint32_t pos;
			std::uint8_t n;
			std::uint8_t stage;
		};

		// The trie walk for one method. pos is the index of the next segment's first byte, or
		// path.size() + 1 once every segment is consumed.
		constexpr std::uint32_t walk(const TableView& t, std::uint32_t root, std::string_view path,
		                             std::array<Capture, kMaxParams>& values, std::uint8_t& nvalues) noexcept
		{
			std::array<Choice, kMaxSegments + 1> stack;  // trivial: never zeroed, only pushed and popped
			std::size_t sp = 0;
			std::uint32_t node = root;
			std::size_t pos = path.size() == 1 ? 2 : 1;
			std::uint8_t nv = 0;
			std::uint8_t stage = 0;
			const std::size_t len = path.size();
			while (true)
			{
				const Node& n = t.nodes[node];
				if (pos > len)
				{
					if (stage == 0 && n.route != kNone)
					{
						nvalues = nv;
						return n.route;
					}
				}
				else
				{
					// The segment at pos: its first word, and the next '/'.
					const std::size_t rem = len - pos;
					const std::uint64_t first = load_at(path, pos);
					std::size_t seglen = first_slash(first, rem);
					if (seglen == 8)
					{
						// No '/' in the first word: read on a word at a time (no call to memchr).
						seglen = rem < 8 ? rem : 8;
						while (seglen < rem)
						{
							const std::size_t k = first_slash(load_at(path, pos + seglen), rem - seglen);
							if (k < 8)
							{
								seglen += k;
								break;
							}
							seglen = seglen + 8 < rem ? seglen + 8 : rem;
						}
					}
					const std::string_view seg(path.data() + pos, seglen);  // in range: no check
					const std::uint64_t word = first & mask_low(seglen);
					const std::size_t next = pos + seglen + 1;
					if (stage == 0 && n.count != 0)
					{
						const std::uint32_t child = find_edge(t, n, seg, word);
						if (child != kNone)
						{
							if (n.branches > 1)
							{
								stack[sp++] = {node, static_cast<std::uint32_t>(pos), nv, 1};
							}
							node = child;
							pos = next;
							continue;
						}
					}
					if (!seg.empty() && nv < kMaxParams)
					{
						// Both typed children absent (the common case) is one test.
						const bool typed = (n.u64 & n.i64) != kNone;
						if (typed && stage <= 1 && n.u64 != kNone && matches_u64(seg))
						{
							if (n.branches > 1)
							{
								stack[sp++] = {node, static_cast<std::uint32_t>(pos), nv, 2};
							}
							values[nv++] = {seg.data(), seg.size()};
							node = n.u64;
							pos = next;
							stage = 0;
							continue;
						}
						if (typed && stage <= 2 && n.i64 != kNone && matches_i64(seg))
						{
							if (n.branches > 1)
							{
								stack[sp++] = {node, static_cast<std::uint32_t>(pos), nv, 3};
							}
							values[nv++] = {seg.data(), seg.size()};
							node = n.i64;
							pos = next;
							stage = 0;
							continue;
						}
						if (stage <= 3 && n.param != kNone)
						{
							if (n.rest != kNone)
							{
								stack[sp++] = {node, static_cast<std::uint32_t>(pos), nv, 4};
							}
							values[nv++] = {seg.data(), seg.size()};
							node = n.param;
							pos = next;
							stage = 0;
							continue;
						}
					}
					if (n.rest != kNone && nv < kMaxParams && pos < len)
					{
						values[nv++] = {path.data() + pos, len - pos};
						nvalues = nv;
						return t.nodes[n.rest].route;
					}
				}
				// A chain node has nothing but its chain, so the walk comes here each time it reaches
				// one; any other node comes here only when it found nothing. The chain's bytes must
				// follow, then a segment boundary; its child is the next node.
				if (n.hashed == kNodeChain && pos <= len)
				{
					const std::size_t stop = pos + n.chain;
					if (stop <= len && (stop == len || path[stop] == '/') && same_run(t.arena, n.edges, path, pos, n.chain))
					{
						++node;
						pos = stop + 1;
						continue;
					}
				}
				if (sp == 0)
				{
					return kNone;
				}
				if !consteval
				{
					MATCHER_ROUTE_ON_BACKTRACK();
				}
				const Choice c = stack[--sp];
				node = c.node;
				pos = c.pos;
				nv = c.n;
				stage = c.stage;
			}
		}

		constexpr std::uint32_t find_in(const TableView& t, unsigned method, std::string_view path,
		                                std::array<Capture, kMaxParams>& values, std::uint8_t& n) noexcept
		{
			n = 0;
			const std::uint32_t lit = find_literal(t, method, path);
			if (lit != kNone)
			{
				return lit;
			}
			const std::uint32_t root = t.roots[method];
			return root == kNone ? kNone : walk(t, root, path, values, n);
		}
	}  // namespace detail

	namespace detail
	{
		// The methods other than `method` under which the path finds a route (for 405). Out of
		// line: its scratch captures would otherwise sit in every lookup's stack frame.
		MATCHER_ROUTE_NOINLINE constexpr std::uint16_t allowed_methods(const TableView& t, unsigned method,
		                                                                 std::string_view path) noexcept
		{
			std::uint16_t allowed = 0;
			std::uint16_t others = static_cast<std::uint16_t>(t.methods & ~(1u << method));
			std::array<Capture, kMaxParams> scratch;
			if consteval
			{
				scratch.fill(Capture{nullptr, 0});
			}
			while (others != 0)
			{
				const unsigned other = static_cast<unsigned>(std::countr_zero(others));
				others = static_cast<std::uint16_t>(others & (others - 1));
				std::uint8_t k = 0;
				if (find_in(t, other, path, scratch, k) != kNone)
				{
					allowed = static_cast<std::uint16_t>(allowed | (1u << other));
				}
			}
			return allowed;
		}
	}  // namespace detail

	// Looks up one request into m, which may hold an earlier result: every field a reader
	// looks at is set again, and the captured values are written, never zeroed. method is an
	// index below kMethodSlots (0 GET, 1 POST, 2 PUT, 3 DELETE, 4 PATCH, 5 HEAD, 6 OPTIONS,
	// 7 CONNECT, 8 TRACE). A caller that keeps a Match of its own fills it through this, so
	// the 256 bytes of captures are written once and never copied.
	constexpr void find_into(const TableView& t, unsigned method, std::string_view path, Match& m) noexcept
	{
		m.status = Status::NotFound;
		m.count = 0;
		m.allowed = 0;
		m.route = kNone;
		if (method >= kMethodSlots || path.empty() || path[0] != '/')
		{
			return;
		}
		m.route = detail::find_in(t, method, path, m.params, m.count);
		if (m.route != kNone)
		{
			m.status = Status::Found;
			return;
		}
		m.count = 0;
		m.allowed = detail::allowed_methods(t, method, path);
		if (m.allowed != 0)
		{
			m.status = Status::MethodNotAllowed;
		}
	}

	// Looks up one request (find_into on a fresh Match).
	constexpr Match find(const TableView& t, unsigned method, std::string_view path) noexcept
	{
		Match m;
		find_into(t, method, path, m);
		return m;
	}

}  // namespace matcher::route
