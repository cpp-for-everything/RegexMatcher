#pragma once

// Route patterns: segments, parameter types, errors, the parser, and the typed-segment tests.

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <matcher/route/config.hpp>

namespace matcher::route
{

	// ========================================================================
	// Patterns
	// ========================================================================

	enum class SegKind : std::uint8_t
	{
		Literal,
		Typed,
		Param,
		Rest
	};

	enum class ParamType : std::uint8_t
	{
		String,
		U64,
		I64
	};

	enum class PatternError : std::uint8_t
	{
		None,
		NoLeadingSlash,
		EmptySegment,
		UnclosedBrace,
		BraceInLiteral,
		EmptyName,
		BadName,
		RepeatedName,
		UnknownType,
		RestNotLast,
		NotPchar,
		TooManySegments,
		TooManyParams,
	};

	constexpr const char* describe(PatternError e) noexcept
	{
		switch (e)
		{
			case PatternError::None: return "no error";
			case PatternError::NoLeadingSlash: return "a route pattern must start with '/'";
			case PatternError::EmptySegment: return "a route pattern has an empty segment";
			case PatternError::UnclosedBrace: return "a route parameter has no closing '}'";
			case PatternError::BraceInLiteral: return "a '{' or '}' inside a literal segment";
			case PatternError::EmptyName: return "a route parameter has an empty name";
			case PatternError::BadName: return "a route parameter name is not [A-Za-z_][A-Za-z0-9_]*";
			case PatternError::RepeatedName: return "a route parameter name is repeated";
			case PatternError::UnknownType: return "a route parameter has an unknown type (string, u64, i64)";
			case PatternError::RestNotLast: return "a catch-all {*name} must be the last segment";
			case PatternError::NotPchar: return "a literal segment has a byte that is not an RFC 3986 pchar";
			case PatternError::TooManySegments: return "a route pattern has more than 32 segments";
			case PatternError::TooManyParams: return "a route pattern has more than 16 parameters";
		}
		return "unknown error";
	}

	struct Segment
	{
		SegKind kind = SegKind::Literal;
		ParamType type = ParamType::String;
		std::string_view text;  // the literal bytes, or the parameter's name
	};

	struct Pattern
	{
		std::array<Segment, kMaxSegments> segs{};
		std::uint8_t n = 0;       // segments; "/" has none
		std::uint8_t params = 0;  // typed, plain and catch-all segments
		PatternError error = PatternError::None;
		std::size_t error_offset = 0;

		constexpr bool ok() const noexcept { return error == PatternError::None; }
		constexpr bool literal() const noexcept { return params == 0; }
		constexpr ParamType param_type(std::size_t i) const noexcept
		{
			for (std::size_t s = 0, k = 0; s < n; ++s)
			{
				if (segs[s].kind != SegKind::Literal && k++ == i)
				{
					return segs[s].type;
				}
			}
			return ParamType::String;
		}
	};

	namespace detail
	{
		constexpr bool is_alpha(char c) noexcept { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
		constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }
		constexpr bool is_hex(char c) noexcept
		{
			return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
		}

		// RFC 3986 pchar, less pct-encoded (checked by the caller).
		constexpr bool is_pchar(char c) noexcept
		{
			if (is_alpha(c) || is_digit(c))
			{
				return true;
			}
			switch (c)
			{
				case '-': case '.': case '_': case '~':                       // unreserved
				case '!': case '$': case '&': case '\'': case '(': case ')':  // sub-delims
				case '*': case '+': case ',': case ';': case '=':
				case ':': case '@':
					return true;
				default:
					return false;
			}
		}

		constexpr bool valid_name(std::string_view s) noexcept
		{
			if (s.empty() || !(is_alpha(s[0]) || s[0] == '_'))
			{
				return false;
			}
			for (char c : s)
			{
				if (!(is_alpha(c) || is_digit(c) || c == '_'))
				{
					return false;
				}
			}
			return true;
		}
	}  // namespace detail

	// Parses "/users/{id:u64}/files/{*path}". Never throws; the result names the first error
	// and the byte offset at which the text stopped being well formed.
	constexpr Pattern parse_pattern(std::string_view text) noexcept
	{
		Pattern p;
		auto fail = [&p](PatternError e, std::size_t at) {
			p.error = e;
			p.error_offset = at;
			return p;
		};
		if (text.empty() || text[0] != '/')
		{
			return fail(PatternError::NoLeadingSlash, 0);
		}
		if (text.size() == 1)
		{
			return p;  // the root
		}
		std::size_t pos = 1;
		while (true)
		{
			std::size_t end = text.find('/', pos);
			if (end == std::string_view::npos)
			{
				end = text.size();
			}
			const std::string_view seg = text.substr(pos, end - pos);
			const bool last = end == text.size();
			if (p.n > 0 && p.segs[p.n - 1].kind == SegKind::Rest)
			{
				return fail(PatternError::RestNotLast, pos);
			}
			if (seg.empty() && !last)
			{
				return fail(PatternError::EmptySegment, pos);
			}
			if (p.n == kMaxSegments)
			{
				return fail(PatternError::TooManySegments, pos);
			}
			Segment s;
			if (!seg.empty() && seg[0] == '{')
			{
				if (seg.back() != '}')
				{
					return fail(PatternError::UnclosedBrace, pos);
				}
				std::string_view inner = seg.substr(1, seg.size() - 2);
				if (inner.find_first_of("{}") != std::string_view::npos)
				{
					return fail(PatternError::BraceInLiteral, pos);
				}
				s.kind = SegKind::Param;
				if (!inner.empty() && inner[0] == '*')
				{
					s.kind = SegKind::Rest;
					inner.remove_prefix(1);
				}
				const std::size_t colon = inner.find(':');
				std::string_view name = inner.substr(0, colon);
				if (colon != std::string_view::npos)
				{
					const std::string_view type = inner.substr(colon + 1);
					if (s.kind == SegKind::Rest)
					{
						return fail(PatternError::UnknownType, pos);
					}
					if (type == "u64")
					{
						s.kind = SegKind::Typed;
						s.type = ParamType::U64;
					}
					else if (type == "i64")
					{
						s.kind = SegKind::Typed;
						s.type = ParamType::I64;
					}
					else if (type != "string")
					{
						return fail(PatternError::UnknownType, pos);
					}
				}
				if (name.empty())
				{
					return fail(PatternError::EmptyName, pos);
				}
				if (!detail::valid_name(name))
				{
					return fail(PatternError::BadName, pos);
				}
				for (std::size_t i = 0; i < p.n; ++i)
				{
					if (p.segs[i].kind != SegKind::Literal && p.segs[i].text == name)
					{
						return fail(PatternError::RepeatedName, pos);
					}
				}
				if (p.params == kMaxParams)
				{
					return fail(PatternError::TooManyParams, pos);
				}
				s.text = name;
				++p.params;
			}
			else
			{
				for (std::size_t i = 0; i < seg.size(); ++i)
				{
					const char c = seg[i];
					if (c == '{' || c == '}')
					{
						return fail(PatternError::BraceInLiteral, pos + i);
					}
					if (c == '%')
					{
						if (i + 2 >= seg.size() || !detail::is_hex(seg[i + 1]) || !detail::is_hex(seg[i + 2]))
						{
							return fail(PatternError::NotPchar, pos + i);
						}
						i += 2;
						continue;
					}
					if (!detail::is_pchar(c))
					{
						return fail(PatternError::NotPchar, pos + i);
					}
				}
				s.text = seg;
			}
			p.segs[p.n++] = s;
			if (last)
			{
				break;
			}
			pos = end + 1;
		}
		return p;
	}

	// ========================================================================
	// Typed segments
	// ========================================================================

	constexpr bool matches_u64(std::string_view s) noexcept
	{
		if (s.empty() || s.size() > 20)
		{
			return false;
		}
		for (char c : s)
		{
			if (!detail::is_digit(c))
			{
				return false;
			}
		}
		return s.size() < 20 || s <= std::string_view("18446744073709551615");
	}

	constexpr bool matches_i64(std::string_view s) noexcept
	{
		const bool neg = !s.empty() && s[0] == '-';
		const std::string_view d = neg ? s.substr(1) : s;
		if (d.empty() || d.size() > 19)
		{
			return false;
		}
		for (char c : d)
		{
			if (!detail::is_digit(c))
			{
				return false;
			}
		}
		return d.size() < 19 || d <= std::string_view(neg ? "9223372036854775808" : "9223372036854775807");
	}

}  // namespace matcher::route
