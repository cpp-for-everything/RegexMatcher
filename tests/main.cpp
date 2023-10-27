#include "../lib/match_graph.hpp"
#include <iostream>

int main(int argc, char** argv) {

    try {
        Regex::Node<int> root;
        std::string regex = "a([a-c]+)b";
        root.process(0, regex.cbegin(), regex.cend());
    }
    catch(const char* str) {
        std::cout << str << std::endl;
    }
    catch(std::exception& e) {
        std::cout << e.what() << std::endl;
    }
    return 0;
}