#include <matcher/core.hpp>
#include <iostream>

#include <chrono>
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::milliseconds;
using namespace std::chrono_literals;

int main(int argc, char** argv)
{
	std::cout << "RegexMatcher VERSION: " << RegexMatcher_VERSION_MAJOR << "." << RegexMatcher_VERSION_MINOR << "." << RegexMatcher_VERSION_PATCH << "."
			  << RegexMatcher_VERSION_TWEAK << std::endl;

	matcher::RegexMatcher<int, char> root;
	int num = 0;
	for (auto x : {"dabcg", "d[a-c][a-c][a-c]g+", "d[a-c]{3,3}g+"})
	{
		std::cout << "Adding " << x << std::endl;
		root.add_regex(std::string(x), num++);
		// root.print();
		std::cout << "Answers:" << std::endl;
		const auto answer = root.match(std::string("dabcg"));
		for (auto x : answer)
		{
			std::cout << x << std::endl;
		}
	}
	return 0;
}
