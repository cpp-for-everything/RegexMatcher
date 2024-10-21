#pragma once
#include <gtest/gtest.h>
#include <matcher/core.hpp>
#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include <regex>
#include <chrono>

using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::milliseconds;
using namespace std::chrono_literals;

void test_for(std::vector<std::string> regexes, std::vector<std::string> texts)
{
	matcher::RegexMatcher<int, char> root;
	std::vector<std::string> dummy;
	int num = 0;
	bool is_it_regex = false;
	std::chrono::nanoseconds total = 0ns, alternative = 0ns;

	for (auto pattern : regexes)
	{
		const auto t1 = high_resolution_clock::now();
		root.add_regex(pattern, num++);
		const auto t2 = high_resolution_clock::now();
		dummy.push_back(pattern);
		const auto t3 = high_resolution_clock::now();
		total = total + (t2 - t1);
		alternative = alternative + (t3 - t2);
	}
	for (auto text : texts)
	{
		const auto t1 = high_resolution_clock::now();
		const auto answer = root.match(text);
		const auto t2 = high_resolution_clock::now();
		std::vector<int> test_result;
		for (size_t i = 0; i < regexes.size(); i++)
		{
			if (std::regex_search(text, std::regex("^(" + regexes[i] + ")$")))
			{
				test_result.push_back(i);
			}
		}
		const auto t3 = high_resolution_clock::now();

		// Time capturing
		total = total + (t2 - t1);
		alternative = alternative + (t3 - t2);

		// Validity check
		EXPECT_EQ(answer, test_result);

		if (answer.size() != test_result.size())
		{
			std::cout << "\tFailed:\n";
			std::cout << "\t" << text << std::endl;
			for (auto x : answer)
			{
				std::cout << "\t\t matcher lib: " << x << ") " << regexes[x - 1] << std::endl;
			}
			std::cout << "\t\t-----------------------\n";
			for (auto x : test_result)
			{
				std::cout << "\t\t  std::regex: " << x << ") " << regexes[x - 1] << std::endl;
			}
			return;
		}
		else
		{
			bool failed = false;
			for (size_t i = 0; i < answer.size(); i++)
			{
				if ((failed = (failed || answer[i] != test_result[i])))
				{
					std::cout << "\t" << text << std::endl;
					std::cout << "\t\t            Failed: " << answer[i] << ") " << regexes[answer[i] - 1] << " | " << test_result[i] << ") "
							  << regexes[test_result[i] - 1] << std::endl;
					return;
				}
			}
			std::cout << "\t" << text << std::endl;
			std::cout << "\t\t          Success: " << duration<double, std::nano>(t2 - t1).count() << "ns (lib) vs "
					  << duration<double, std::nano>(t3 - t2).count() << "ns (std::regex)" << std::endl;
		}
	}

	if (total > alternative + alternative / 10)
	{
		std::cout << "\t\t Success but slow: " << total.count() << "ns (lib) vs " << alternative.count() << "ns (std::regex)" << std::endl;
	}
}
