// The word helpers give the same bytes at run time as in constant evaluation.
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>

#include <matcher/route.hpp>

using namespace matcher::route;

TEST(RouteWords, WordLoadsGiveTheSameBytesAtRunTimeAsInConstantEvaluation)
{
	constexpr auto by_bytes = [](std::string_view s, std::size_t pos) {
		std::uint64_t w = 0;
		for (std::size_t i = 0; i < 8 && pos + i < s.size(); ++i)
		{
			w |= static_cast<std::uint64_t>(static_cast<unsigned char>(s[pos + i])) << (8 * i);
		}
		return w;
	};
	static_assert(detail::load_at("/abc", 1) == by_bytes("/abc", 1));
	for (const std::string s : {"", "/", "/a/b", "/abcdefg", "/abcdefgh", "/users/12345/posts/a-long-slug-name"})
	{
		for (std::size_t pos = 0; pos <= s.size(); ++pos)
		{
			SCOPED_TRACE(s + " at " + std::to_string(pos));
			EXPECT_EQ(detail::load_at(s, pos), by_bytes(s, pos));
		}
	}
}
