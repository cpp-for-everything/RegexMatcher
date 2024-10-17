#ifndef REGEX_MATCHER_RANGE_H
#define REGEX_MATCHER_RANGE_H

namespace matcher::regex
{
	/**
	 * @brief Wrapper class for symbol ranges of type [from, to]
	 *
	 * @tparam symbol_t Symbol type
	 */
	template <typename symbol_t> class Range
	{
		// the range is [from, to]
		symbol_t from;
		symbol_t to;

	public:
		/**
		 * @brief Construct an empty Range object
		 *
		 */
		Range();

		/**
		 * @brief Construct a new Range object from a single character
		 *
		 * @param ch the character
		 */
		explicit Range(symbol_t ch);

		/**
		 * @brief Construct a new Range object with range [from, to]
		 *
		 * @param from starting character
		 * @param to ending character (incl. this one)
		 */
		explicit Range(symbol_t from, symbol_t to);

		/**
		 * @brief Checks if a given character is in the current range
		 *
		 * @param ch character in question
		 * @return true if the character is in the range [from, to]
		 * @return false if the character is not in the range [from, to]
		 */
		bool contains(symbol_t ch);

		/**
		 * @brief Comparison operator
		 *
		 */
		auto operator<=>(const Range<symbol_t>& other) const;
	};
} // namespace matcher::regex

#include "../src/range.cpp"

#endif
