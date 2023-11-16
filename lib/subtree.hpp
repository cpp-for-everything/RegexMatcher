#ifndef SUBTREE_H
#define SUBTREE_H

#include <vector>

namespace URLMatcher {
    template<typename Node>
    class SubTree {
    public:
        std::vector<Node*> roots;
        std::vector<Node*> leafs;

        SubTree(std::vector<Node*> a, std::vector<Node*> b) : roots(a), leafs(b) {}
        SubTree() : roots(), leafs() {}

        inline const std::vector<Node*>& get_roots() const {
            return roots;
        }

        inline const std::vector<Node*>& get_leafs() const {
            return leafs;
        }
    };
}

#endif