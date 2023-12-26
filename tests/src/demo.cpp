#include <matcher/core.hpp>
#include <iostream>

#include <chrono>
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::milliseconds;
using namespace std::chrono_literals;

int main(int argc, char** argv) {
    std::cout << "RegexMatcher VERSION: " << RegexMatcher_VERSION_MAJOR << "." << RegexMatcher_VERSION_MINOR << "." << RegexMatcher_VERSION_PATCH << "." << RegexMatcher_VERSION_TWEAK << std::endl;
    
    matcher::RegexMatcher<int, char> root;
    root.add_regex(std::string("a|b"), 0);
    root.print();
    root.add_regex(std::string("a+"), 1);
    root.print();
    const auto answer = root.match(std::string("ccc"));
    return 0;
}