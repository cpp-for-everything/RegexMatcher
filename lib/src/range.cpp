#include "range.hpp"

namespace matcher::regex
{
	template <typename symbol_t> Range<symbol_t>::Range() : Range(symbol_t() + 1, symbol_t())
	{
	}

	template <typename symbol_t> Range<symbol_t>::Range(symbol_t ch) : from(ch), to(ch)
	{
	}

	template <typename symbol_t> Range<symbol_t>::Range(symbol_t from, symbol_t to) : from(from), to(to)
	{
	}

	template <typename symbol_t> bool Range<symbol_t>::contains(symbol_t ch)
	{
		return from <= ch && ch <= to;
	}

	template <typename symbol_t> auto Range<symbol_t>::operator<=>(const Range<symbol_t>& other) const
	{
		return from <=> other.from;
	}
} // namespace matcher::regex
