#include <gtest/gtest.h>
#include "node_info.hpp"
#include "range.hpp"

// Test cases
TEST(CreateRange, SingleSymbol)
{
	auto range = matcher::regex::Range<char>('a');
	EXPECT_EQ(range.contains('a'), true);
	EXPECT_EQ(range.contains('b'), false);
	EXPECT_EQ(range.contains('A'), false);
}

TEST(CreateRange, Empty)
{
	matcher::regex::Range<char> range;
	EXPECT_EQ(range.contains('a'), false);
	EXPECT_EQ(range.contains('b'), false);
	EXPECT_EQ(range.contains('A'), false);
	EXPECT_EQ(range.contains('\0'), false);
}

TEST(CreateRange, FromAtoZ)
{
	auto range = matcher::regex::Range<char>('A', 'Z');
	EXPECT_EQ(range.contains('a'), false);
	EXPECT_EQ(range.contains('b'), false);
	for (char ch = 'A'; ch <= 'Z'; ch++)
	{
		EXPECT_EQ(range.contains(ch), true);
	}
	EXPECT_EQ(range.contains('\0'), false);
}

int main(int argc, char** argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
