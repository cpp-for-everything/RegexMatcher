#include <regex_matcher.hpp>
#include <iostream>

int main(int argc, char** argv)
{
	std::cout << "RegexMatcher VERSION: " << matcher::RegexMatcher_VERSION_MAJOR << "." << matcher::RegexMatcher_VERSION_MINOR << "."
			  << matcher::RegexMatcher_VERSION_PATCH << "." << matcher::RegexMatcher_VERSION_TWEAK << std::endl;

	matcher::RegexMatcher<int, char> root;
	root.add_regex(std::string("a|b"), 0);
	root.print(std::cout);
	root.add_regex(std::string("a+"), 1);
	root.print(std::cout);
	const auto answer = root.match(std::string("ccc"));
	return 0;
}
