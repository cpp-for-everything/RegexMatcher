#include "../lib/node.hpp"
#include <iostream>

int main(int argc, char** argv) {
    {
        Node root = Node();
        EdgeInfo_t edge;
        edge.paths.emplace(1, Limits::common_edge);
        edge.to = new Node('a');
        std::cout << edge.to->to_string() << "(" << edge.to << ")" << "\n";
        root.neighbours.emplace('a', edge);
        std::string regex = "a[a-c]";
        auto x = Node::processSet({&root}, 1, regex.begin() + 1);
        for (auto a : x) {
            std::cout << a->to_string() << "(" << a << ")" << " ";
        }
    }
    return 0;
}