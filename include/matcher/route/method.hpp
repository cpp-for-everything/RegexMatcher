#pragma once

// HTTP methods as the table's method index, and the lookup by method.

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <matcher/route/config.hpp>
#include <matcher/route/lookup.hpp>
#include <matcher/route/table.hpp>

namespace matcher::route
{

	// The value of each method is its index in a table (RouteSpec::method, find's method).
	enum class Method : std::uint8_t
	{
		Get,
		Post,
		Put,
		Delete,
		Patch,
		Head,
		Options,
		Connect,
		Trace
	};
	static_assert(static_cast<std::size_t>(Method::Trace) + 1 == kMethodSlots);

	constexpr unsigned index(Method m) noexcept
	{
		return static_cast<unsigned>(m);
	}

	// find and find_into by method: they forward to the index form.
	constexpr Match find(const TableView& t, Method m, std::string_view path) noexcept
	{
		return find(t, index(m), path);
	}

	constexpr void find_into(const TableView& t, Method m, std::string_view path, Match& out) noexcept
	{
		find_into(t, index(m), path, out);
	}

}  // namespace matcher::route
