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
        std::string regex =  "f[a-c]d(asdf)g";
        std::string regex2 = "f[c-d]d(asd)h";
        std::vector<Node*> leafs;
        {
            auto it = regex.begin();
            process({&root}, 1, it, regex.end(), false, leafs);
        }
        {
            auto it = regex2.begin();
            process({&root}, 2, it, regex2.end(), false, leafs);
        }
        root.print();
    }
    return 0;
}