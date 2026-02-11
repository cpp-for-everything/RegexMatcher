#include <matcher/core.hpp>
#include <iostream>

#include <chrono>
using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::high_resolution_clock;
using std::chrono::milliseconds;
using namespace std::chrono_literals;

#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

int main(int argc, char** argv) {
	std::cout << "RegexMatcher VERSION: " << STRINGIFY(RegexMatcher_VERSION_MAJOR) << "."
	          << STRINGIFY(RegexMatcher_VERSION_MINOR) << "." << STRINGIFY(RegexMatcher_VERSION_PATCH) << "."
	          << STRINGIFY(RegexMatcher_VERSION_TWEAK) << std::endl;

	matcher::RegexMatcher<int, char> root;
	int num = 0;
	for (auto x : {"dabcg", "d[a-c][a-c][a-c]g+", "d[a-c]{3,3}g+"}) {
		std::cout << "Adding " << x << std::endl;
		root.add_regex(std::string(x), num++);
		// root.print();
		std::cout << "Answers:" << std::endl;
		const auto answer = root.match(std::string("dabcg"));
		for (auto x : answer) {
			std::cout << x << std::endl;
		}
	}
	return 0;
}
