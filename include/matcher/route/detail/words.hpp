#pragma once

// Bytes, words and hashes: the same results in constant evaluation and at run time.

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

#include <matcher/route/config.hpp>

namespace matcher::route
{

	// ========================================================================
	// Bytes, words and hashes (the same results in constant evaluation and at run time)
	// ========================================================================

	namespace detail
	{
		inline constexpr std::uint64_t kSlashes = 0x2F2F'2F2F'2F2F'2F2FULL;
		inline constexpr std::uint64_t kLow = 0x0101'0101'0101'0101ULL;
		inline constexpr std::uint64_t kHigh = 0x8080'8080'8080'8080ULL;

		// Up to 8 bytes of s from pos, little-endian, zero-padded; never reads outside s.
		// At run time one 8-byte load: at pos when 8 bytes remain, otherwise the last 8 bytes
		// of s shifted down, so a copy of variable length is never needed.
		constexpr std::uint64_t load_at(std::string_view s, std::size_t pos) noexcept
		{
			const std::size_t rem = s.size() - pos;
			if (rem == 0)
			{
				return 0;
			}
			if !consteval
			{
				if constexpr (std::endian::native == std::endian::little)
				{
					std::uint64_t w;
					if (rem >= 8)
					{
						std::memcpy(&w, s.data() + pos, 8);
						return w;
					}
					if (s.size() >= 8)
					{
						std::memcpy(&w, s.data() + s.size() - 8, 8);
						return w >> (8 * (8 - rem));
					}
				}
			}
			const std::size_t n = rem < 8 ? rem : 8;
			std::uint64_t w = 0;
			for (std::size_t i = 0; i < n; ++i)
			{
				w |= static_cast<std::uint64_t>(static_cast<unsigned char>(s[pos + i])) << (8 * i);
			}
			return w;
		}

		// Whether a and b, both n bytes long, have the same bytes from index `from` on (the
		// bytes before it are known to be equal). At run time, for n >= 8, whole 8-byte words
		// and one last word that ends at n and may overlap the one before: no call, no byte
		// loop.
		MATCHER_ROUTE_INLINE constexpr bool same_from(const char* a, const char* b, std::size_t n,
		                                                std::size_t from) noexcept
		{
			if !consteval
			{
				if (n >= 8)
				{
					const auto word = [](const char* p) {
						std::uint64_t w;
						std::memcpy(&w, p, 8);
						return w;
					};
					for (std::size_t i = from; i + 8 < n; i += 8)
					{
						if (word(a + i) != word(b + i))
						{
							return false;
						}
					}
					return word(a + n - 8) == word(b + n - 8);
				}
			}
			const std::string_view x(a, n);
			const std::string_view y(b, n);
			for (std::size_t i = from; i < n; i += 8)
			{
				if (load_at(x, i) != load_at(y, i))
				{
					return false;
				}
			}
			return true;
		}

		// The index of the first '/' among the first n (at most 8) bytes of w, or 8.
		constexpr std::size_t first_slash(std::uint64_t w, std::size_t n) noexcept
		{
			const std::uint64_t x = w ^ kSlashes;
			const std::uint64_t z = (x - kLow) & ~x & kHigh;
			const std::size_t i = z ? static_cast<std::size_t>(std::countr_zero(z)) / 8 : 8;
			return i < n ? i : 8;
		}

		constexpr std::uint64_t mask_low(std::size_t bytes) noexcept
		{
			return bytes >= 8 ? ~0ULL : ((1ULL << (8 * bytes)) - 1);
		}

		constexpr std::uint64_t mix(std::uint64_t h) noexcept
		{
			h ^= h >> 32;
			h *= 0xD6E8'FEB8'6659'FD93ULL;
			h ^= h >> 32;
			return h;
		}

		// A hash of a whole path and a method, read in 8-byte words.
		constexpr std::uint64_t path_hash(std::string_view s, unsigned method) noexcept
		{
			std::uint64_t h = 0x9E37'79B9'7F4A'7C15ULL ^ (static_cast<std::uint64_t>(method) << 56) ^ s.size();
			std::size_t i = 0;
			for (; i + 8 <= s.size(); i += 8)
			{
				h = (h ^ load_at(s, i)) * 0x9E37'79B9'7F4A'7C15ULL;
				h ^= h >> 29;
			}
			if (i < s.size())
			{
				h = (h ^ load_at(s, i)) * 0x9E37'79B9'7F4A'7C15ULL;
			}
			return mix(h);
		}

		// The slot hash of a literal edge: its length, its first word and its last word.
		constexpr std::uint64_t edge_hash(std::uint64_t first, std::string_view s) noexcept
		{
			std::uint64_t h = first * 0x9E37'79B9'7F4A'7C15ULL ^ s.size();
			if (s.size() > 8)
			{
				h ^= load_at(s, s.size() - 8) * 0xC2B2'AE3D'27D4'EB4FULL;
			}
			return mix(h);
		}
	}  // namespace detail

}  // namespace matcher::route
