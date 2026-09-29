// Route patterns: parsing into segments, and the error and offset of every malformed form.
#include <gtest/gtest.h>

#include <matcher/route.hpp>

using namespace matcher::route;

TEST(RoutePattern, ParsesIntoSegments)
{
	const Pattern p = parse_pattern("/users/{id:u64}/files/{*path}");
	ASSERT_TRUE(p.ok());
	ASSERT_EQ(p.n, 4);
	EXPECT_EQ(p.params, 2);
	EXPECT_EQ(p.segs[0].kind, SegKind::Literal);
	EXPECT_EQ(p.segs[0].text, "users");
	EXPECT_EQ(p.segs[1].kind, SegKind::Typed);
	EXPECT_EQ(p.segs[1].type, ParamType::U64);
	EXPECT_EQ(p.segs[1].text, "id");
	EXPECT_EQ(p.segs[3].kind, SegKind::Rest);
	EXPECT_EQ(p.segs[3].text, "path");
	EXPECT_TRUE(parse_pattern("/").ok());
	EXPECT_EQ(parse_pattern("/").n, 0);
	EXPECT_EQ(parse_pattern("/a/").n, 2);  // a trailing slash is a final empty literal
	EXPECT_TRUE(parse_pattern("/caf%C3%A9/{x:string}").ok());
	EXPECT_TRUE(parse_pattern("/a:b@c/!$&'()*+,;=-._~").ok());
}

TEST(RoutePattern, MalformedPatternsNameTheirErrorAndOffset)
{
	struct Case
	{
		const char* text;
		PatternError error;
		std::size_t offset;
	};
	const Case cases[] = {
		{"users", PatternError::NoLeadingSlash, 0},
		{"/a//b", PatternError::EmptySegment, 3},
		{"/a/{id", PatternError::UnclosedBrace, 3},
		{"/a/{}", PatternError::EmptyName, 3},
		{"/a/{9x}", PatternError::BadName, 3},
		{"/a/{id}/b/{id}", PatternError::RepeatedName, 10},
		{"/a/{id:float}", PatternError::UnknownType, 3},
		{"/f/{*p}/x", PatternError::RestNotLast, 8},
		{"/a b", PatternError::NotPchar, 2},
		{"/a#b", PatternError::NotPchar, 2},
		{"/a%zz", PatternError::NotPchar, 2},
		{"/a{b}", PatternError::BraceInLiteral, 2},
	};
	for (const Case& c : cases)
	{
		SCOPED_TRACE(c.text);
		const Pattern p = parse_pattern(c.text);
		EXPECT_EQ(p.error, c.error);
		EXPECT_EQ(p.error_offset, c.offset);
		EXPECT_STRNE(describe(p.error), describe(PatternError::None));
	}
}
