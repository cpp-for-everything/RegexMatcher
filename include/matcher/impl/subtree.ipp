/**
 * @file subtree.ipp
 * @brief Template implementation for SubTree class
 * @note This file is included by core.hpp - do not include directly
 */

namespace {

	template <typename Node_T>
	inline const std::vector<Node_T*>& SubTree<Node_T>::get_roots() const {
		return roots;
	}

	template <typename Node_T>
	inline const std::vector<Node_T*>& SubTree<Node_T>::get_leafs() const {
		return leafs;
	}

}  // anonymous namespace
