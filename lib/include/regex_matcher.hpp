#ifndef REGEX_MATCHER_H
#define REGEX_MATCHER_H

#include <iostream>
#include <memory>
#include <vector>
#include "RegexMatcherConfig.h"

namespace matcher
{
	template <typename RegexData, typename symbol_t = char> class RegexMatcher
	{
	public:
		// Constructor
		RegexMatcher();

		// Destructor
		~RegexMatcher();

		// Deleted copy constructor and assignment operator to enforce unique ownership
		RegexMatcher(const RegexMatcher&) = delete;
		RegexMatcher& operator=(const RegexMatcher&) = delete;

		// Move constructor and assignment operator
		RegexMatcher(RegexMatcher&&) noexcept;
		RegexMatcher& operator=(RegexMatcher&&) noexcept;

		// Public methods
		/**
		 * @brief Adds new regex to match against
		 *
		 * @tparam Iterable Iterable container of symbols
		 * @param regex Iterable container of the symbols of the regex
		 * @param user_data User data associated with the given regex
		 * @return int Number of new nodes created in the trie. When 0, the regex match 1-to-1 previously added regex
		 */
		template <typename Iterable> int add_regex(const Iterable& regex, const RegexData& user_data);

		/**
		 * @brief Matches given set of symbols against all previously added regexes
		 *
		 * @tparam Iterable Iterable container of the symbols of the string for matching
		 * @param regex Iterable container of the symbols of the string for matching
		 * @return std::vector<RegexData> All regexes that match the given string
		 */
		template <typename Iterable> std::vector<RegexData> match(const Iterable& regex);

		/**
		 * @brief Print the graph structure (for debug purposes)
		 *
		 * @param out output stream where the result should be printed
		 */
		void print(std::ostream& out);

	private:
		// Forward declaration of RegexMatcher's implementation
		class RegexMatcherImpl;

		// pImpl pointer to hide the implementation details
		std::unique_ptr<RegexMatcherImpl> impl;
	};
} // namespace matcher

#include "../src/regex_matcher.cpp"

#endif
