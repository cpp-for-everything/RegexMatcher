#pragma once

// A table built while compiling: a constexpr object with the run-time table's layout.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include <matcher/route/table.hpp>
#include <matcher/route/lookup.hpp>

namespace matcher::route
{

	// ========================================================================
	// A table that is a compile-time constant
	// ========================================================================

	// The four arrays lie where table_layout(S) puts them. Declared alignas(kPageAlign), the
	// table has every element at the offset into a page that a RuntimeTable of the same routes
	// has, so that neither table's placement favours its lookups:
	//     alignas(kPageAlign) static constexpr auto table = make_static_table<routes>();
	template <StaticSizes S>
	struct StaticTable
	{
		alignas(kTableAlign) std::array<Node, S.nodes> nodes{};
		alignas(kTableAlign) std::array<Edge, S.edges> edges{};
		alignas(kTableAlign) std::array<LiteralSlot, S.literals> literals{};
		alignas(kTableAlign) std::array<char, S.arena + 1> arena{};
		std::array<std::uint32_t, kMethodSlots> roots = no_roots();
		std::uint16_t methods = 0;
		std::uint16_t literal_methods = 0;

		constexpr TableView view() const noexcept
		{
			return {nodes, edges, literals, std::string_view(arena.data(), S.arena), roots, methods, literal_methods};
		}
		constexpr Match find(unsigned method, std::string_view path) const noexcept
		{
			return route::find(view(), method, path);
		}
	};

	namespace detail
	{
		// Named so that the compiler's message says what is wrong with a compile-time table.
		inline void compile_time_table_has_a_bad_pattern() {}
		inline void compile_time_table_has_a_bad_method() {}
		inline void compile_time_table_has_duplicate_routes() {}
		inline void compile_time_table_is_too_large() {}

		template <auto& Specs>
		consteval Built checked_build()
		{
			Built b = build_table(std::span<const RouteSpec>(std::data(Specs), std::size(Specs)));
			switch (b.error)
			{
				case BuildError::None: break;
				case BuildError::BadPattern: compile_time_table_has_a_bad_pattern(); break;
				case BuildError::BadMethod: compile_time_table_has_a_bad_method(); break;
				case BuildError::Duplicate: compile_time_table_has_duplicate_routes(); break;
				case BuildError::TooLarge: compile_time_table_is_too_large(); break;
			}
			return b;
		}

		template <auto& Specs>
		consteval StaticSizes static_sizes()
		{
			const Built b = checked_build<Specs>();
			return {b.nodes.size(), b.edges.size(), b.literals.size(), b.arena.size()};
		}
	}  // namespace detail

	// A routing table built while compiling, from a constexpr array of RouteSpec:
	//     static constexpr RouteSpec routes[] = {{0, "/users/{id}", 0}, ...};
	//     static constexpr auto table = make_static_table<routes>();
	// A malformed pattern or a duplicate route stops the compilation.
	template <auto& Specs>
	consteval auto make_static_table()
	{
		constexpr StaticSizes sizes = detail::static_sizes<Specs>();
		using T = StaticTable<sizes>;
		static_assert(std::is_standard_layout_v<T>);
		static_assert(offsetof(T, nodes) == table_layout(sizes).nodes && offsetof(T, edges) == table_layout(sizes).edges &&
		                  offsetof(T, literals) == table_layout(sizes).literals &&
		                  offsetof(T, arena) == table_layout(sizes).arena,
		              "a compile-time table's arrays must lie where table_layout puts a run-time table's");
		const Built b = detail::checked_build<Specs>();
		T t;
		std::copy(b.nodes.begin(), b.nodes.end(), t.nodes.begin());
		std::copy(b.edges.begin(), b.edges.end(), t.edges.begin());
		std::copy(b.literals.begin(), b.literals.end(), t.literals.begin());
		std::copy(b.arena.begin(), b.arena.end(), t.arena.begin());
		t.roots = b.roots;
		t.methods = b.methods;
		t.literal_methods = b.literal_methods;
		return t;
	}

}  // namespace matcher::route
