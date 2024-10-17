#include "regex_matcher.hpp"
#include "regex_matcher_impl.hpp"

namespace matcher
{
	template <typename RegexData, typename symbol_t> RegexMatcher<RegexData, symbol_t>::RegexMatcher() : impl(std::make_unique<RegexMatcherImpl>())
	{
	}

	template <typename RegexData, typename symbol_t> RegexMatcher<RegexData, symbol_t>::~RegexMatcher() = default;

	template <typename RegexData, typename symbol_t>
	template <typename Iterable>
	int RegexMatcher<RegexData, symbol_t>::add_regex(const Iterable& regex, const RegexData& user_data)
	{
		return 0;
	}

	template <typename RegexData, typename symbol_t>
	template <typename Iterable>
	std::vector<RegexData> RegexMatcher<RegexData, symbol_t>::match(const Iterable& regex)
	{
		return {};
	}

	template <typename RegexData, typename symbol_t> void RegexMatcher<RegexData, symbol_t>::print(std::ostream& out)
	{
		out << "Sike!\n";
		return;
	}

} // namespace matcher
