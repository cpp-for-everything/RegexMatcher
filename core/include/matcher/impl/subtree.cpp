#ifndef SUBTREE_IMPL
#define SUBTREE_IMPL

#include <matcher/core.hpp>

namespace {
    template<typename Node_T>
    inline const std::vector<Node_T*>& SubTree<Node_T>::get_roots() const {
        return roots;
    }

    template<typename Node_T>
    inline const std::vector<Node_T*>& SubTree<Node_T>::get_leafs() const {
        return leafs;
    }
}

#endif
