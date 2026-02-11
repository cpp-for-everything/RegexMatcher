/**
 * @file core.ipp
 * @brief Template implementation for RegexMatcher class
 * @note This file is included by core.hpp - do not include directly
 */

#include <list>

namespace matcher {

	template <typename RegexData, typename char_t>
	template <typename ConstIterator>
	Limits* RegexMatcher<RegexData, char_t>::processLimit(const SubTree<Node<RegexData, char_t>>& parent_of_latest,
	                                                      SubTree<Node<RegexData, char_t>>& lastest, RegexData regex,
	                                                      ConstIterator& it,
	                                                      std::vector<std::unique_ptr<Limits>>& limits_storage) {
		if (*it != '{')  // not called at the beginning of a set
		{
			throw std::logic_error("The iterator doesn't start from a limit group.");
		} else {
			it++;
		}

		auto answer_unique = std::make_unique<Limits>(Limits::common_edge);
		auto answer = answer_unique.get();
		limits_storage.push_back(std::move(answer_unique));

		bool min = true;
		size_t number = 0;

		number = 0;
		while (*it != '}') {
			if (*it == ',') {
				min = false;
				answer->min = number;
				number = 0;
			} else {  // it is a digit
				number = number * 10 + (*it - '0');
			}
			it++;
		}

		if (!min && number != 0) {
			answer->max = number;
		}
		if (!min && number == 0) {
			answer->max = std::nullopt;
		}
		if (min) {
			answer->max = number;
		}

		const size_t leafs = lastest.get_leafs().size();

		if (answer->min == 0) {
			for (auto root : parent_of_latest.get_leafs()) {
				lastest.leafs.push_back(root);
			}
			answer->min = 1;
		}
		answer->min = answer->min - 1;
		if (answer->max.has_value()) {
			answer->max = answer->max.value() - 1;
		}

		if (answer->is_allowed_to_repeat()) {
			for (size_t i = 0; i < leafs; i++) {
				for (auto root : lastest.get_roots()) {
					// if another child with same symbol exists
					if (lastest.get_leafs()[i]->hasChild(root->current_symbol) &&
					    lastest.get_leafs()[i]->neighbours[root->current_symbol].to != root) {
						Node<RegexData, char_t>* old_child =
						    lastest.get_leafs()[i]->neighbours[root->current_symbol].to;
						root->absorb(old_child);
						lastest.get_leafs()[i]->neighbours[root->current_symbol].to = root;
					}
					lastest.get_leafs()[i]->connect_with(root, regex, limits_storage, answer);
				}
			}
		}

		return answer;
	}

	template <typename RegexData, typename char_t>
	template <typename ConstIterator>
	SubTree<Node<RegexData, char_t>> RegexMatcher<RegexData, char_t>::processSet(
	    std::vector<Node<RegexData, char_t>*> parents, [[maybe_unused]] RegexData regex, ConstIterator& it,
	    std::vector<std::unique_ptr<Node<RegexData, char_t>>>& nodes_storage) {
		if (*it != '[')  // not called at the beginning of a set
		{
			throw std::logic_error("The iterator doesn't start from a set group.");
		} else {
			it++;
		}
		std::vector<Node<RegexData, char_t>*> leafs;
		ConstIterator prev;
		bool takeTheNextSymbolLitterally = false;
		while (*it != ']' || takeTheNextSymbolLitterally) {
			if (!takeTheNextSymbolLitterally) {
				if (*it == '\\') {  // escape symbol is always followed by a reglar character
					it++;           // so it is included no matter what
					takeTheNextSymbolLitterally = true;
				} else if (*it == '-') {
					it++;
					for (char ch = ((*prev) + 1); ch <= *it; ch++) {
						Node<RegexData, char_t>* nextLeaf = nullptr;
						for (auto parent : parents) {
							if (parent->hasChild(ch)) {
								nextLeaf = parent->getChild(ch);
								break;
							}
						}
						if (nextLeaf == nullptr) {
							nodes_storage.push_back(std::make_unique<Node<RegexData, char_t>>(ch));
							nextLeaf = nodes_storage.back().get();
						}
						leafs.push_back(nextLeaf);
					}
				}
				// TODO: implement not
				else {
					takeTheNextSymbolLitterally = true;
				}
			}
			if (takeTheNextSymbolLitterally) {
				Node<RegexData, char_t>* nextLeaf = nullptr;
				for (auto parent : parents) {
					if (parent->hasChild(*it)) {
						nextLeaf = parent->getChild(*it);
						break;
					}
				}
				if (nextLeaf == nullptr) {
					nodes_storage.push_back(std::make_unique<Node<RegexData, char_t>>(*it));
					nextLeaf = nodes_storage.back().get();
				}
				leafs.push_back(nextLeaf);
				takeTheNextSymbolLitterally = false;
			}
			prev = it;
			it++;
		}
		return {leafs, leafs};
	}

	template <typename RegexData, typename char_t>
	template <typename ConstIterator>
	SubTree<Node<RegexData, char_t>> RegexMatcher<RegexData, char_t>::process(
	    std::vector<Node<RegexData, char_t>*> parents, RegexData regex, ConstIterator& it, ConstIterator end,
	    const bool inBrackets, size_t& group_counter, std::vector<TagAction>& pending_actions,
	    std::vector<std::unique_ptr<Node<RegexData, char_t>>>& nodes_storage,
	    std::vector<std::unique_ptr<Limits>>& limits_storage) {
		SubTree<Node<RegexData, char_t>> answer = {{}, {}};
		std::vector<SubTree<Node<RegexData, char_t>>> nodeLayers = {{parents, parents}};
		// Save initial pending actions to restore on alternation
		std::vector<TagAction> initial_actions = pending_actions;
		for (; it != end; it++) {
			if (*it == ')' && inBrackets) {
				break;
			}
			if (*it == '[') {  // start of a set
				const auto latest_parents = nodeLayers.back();
				SubTree<Node<RegexData, char_t>> newNodes =
				    processSet(latest_parents.get_leafs(), regex, it, nodes_storage);
				for (auto parent : latest_parents.get_leafs()) {
					for (auto newNode : newNodes.get_leafs()) {
						parent->connect_with(newNode, regex, pending_actions, limits_storage);
					}
				}
				pending_actions.clear();
				nodeLayers.push_back(newNodes);
			} else if (*it == '(') {  // start of a regex in brackets (capture group)
				size_t current_group_id = group_counter++;
				pending_actions.push_back(TagAction::open(current_group_id));  // OPEN_GROUP tag
				it++;
				SubTree<Node<RegexData, char_t>> newLayer =
				    process(nodeLayers.back().get_leafs(), regex, it, end, true, group_counter, pending_actions,
				            nodes_storage, limits_storage);                     // leaves it at the closing bracket
				pending_actions.push_back(TagAction::close(current_group_id));  // CLOSE_GROUP tag
				nodeLayers.push_back(newLayer);
			} else if (*it == '|') {
				answer.roots.insert(answer.roots.end(), nodeLayers[1].get_leafs().begin(),
				                    nodeLayers[1].get_leafs().end());
				answer.leafs.insert(answer.leafs.end(), nodeLayers.back().get_leafs().begin(),
				                    nodeLayers.back().get_leafs().end());
				nodeLayers.resize(1);
				// Restore initial actions for the next alternative branch
				pending_actions = initial_actions;
			} else if (*it == '{') {
				[[maybe_unused]] Limits* limits =
				    processLimit(nodeLayers[nodeLayers.size() - 2], nodeLayers.back(), regex, it, limits_storage);
			} else if (auto special_regex = Node<RegexData, char_t>::special_symbols.find(*it);
			           special_regex != Node<RegexData, char_t>::special_symbols.end()) {
				auto tmp_it = special_regex->second.cbegin();
				[[maybe_unused]] Limits* limits =
				    processLimit(nodeLayers[nodeLayers.size() - 2], nodeLayers.back(), regex, tmp_it, limits_storage);
			} else {  // normal character
				symbol<char_t> sym;
				if (*it == '\\') {  // skip escape symbol
					it++;
					sym = symbol<char_t>(*it);
				} else if (*it == '.') {
					sym = symbol<char_t>::Any;
				} else {
					sym = symbol<char_t>(*it);
				}
				Node<RegexData, char_t>* nextNode = nullptr;
				for (auto parent : nodeLayers.back().get_leafs()) {
					if (parent->neighbours.find(sym) != parent->neighbours.end()) {
						nextNode = parent->neighbours[sym].to;
						break;
					}
				}
				if (nextNode == nullptr) {
					nodes_storage.push_back(std::make_unique<Node<RegexData, char_t>>(sym));
					nextNode = nodes_storage.back().get();
				}
				for (auto parent : nodeLayers.back().get_leafs()) {
					parent->connect_with(nextNode, regex, pending_actions, limits_storage);
				}
				pending_actions.clear();
				nodeLayers.push_back({{nextNode}, {nextNode}});
			}
		}
		answer.roots.insert(answer.roots.end(), nodeLayers[1].get_leafs().begin(), nodeLayers[1].get_leafs().end());
		answer.leafs.insert(answer.leafs.end(), nodeLayers.back().get_leafs().begin(),
		                    nodeLayers.back().get_leafs().end());
		if (it == end) {
			nodes_storage.push_back(std::make_unique<Node<RegexData, char_t>>(symbol<char_t>::EOR));
			Node<RegexData, char_t>* end_of_regex = nodes_storage.back().get();
			SubTree<Node<RegexData, char_t>> final_answer = {answer.get_roots(), {end_of_regex}};
			for (auto parent : answer.leafs) {
				parent->connect_with(end_of_regex, regex, pending_actions, limits_storage);
			}
			return final_answer;
		}

		return answer;
	}

	template <typename RegexData, typename char_t>
	template <typename Iterable>
	void RegexMatcher<RegexData, char_t>::add_regex(Iterable str, RegexData uid) {
		auto it = std::cbegin(str);
		size_t group_counter = 0;
		std::vector<TagAction> pending_actions;
		process(std::vector{&root}, uid, it, std::cend(str), false, group_counter, pending_actions, nodes_storage,
		        limits_storage);
	}

	template <typename RegexData, typename char_t>
	template <typename Iterable>
	std::vector<RegexData> RegexMatcher<RegexData, char_t>::match(Iterable str) const {
		return root.match(std::cbegin(str), std::cend(str));
	}

	template <typename RegexData, typename char_t>
	template <typename Iterable>
	std::vector<MatchResult<RegexData>> RegexMatcher<RegexData, char_t>::match_with_groups(Iterable str) const {
		return root.match_with_groups(std::cbegin(str), std::cend(str));
	}

}  // namespace matcher
