#include "../lib/RegexMatcher.hpp"

namespace URLMatcher {

    template<typename UniqueMatchDataPtr, typename symbol_t>
    template<typename ConstIterator>
    Container<std::pair<Limits, size_t>>::const_iterator RegexMatcher<UniqueMatchDataPtr, symbol_t>::processLimit(const SubTree<Node<UniqueMatchDataPtr, symbol_t>>& parent_of_latest, SubTree<Node<UniqueMatchDataPtr, symbol_t>>& lastest, UniqueMatchDataPtr regex, ConstIterator& it) {
        if (*it != '{') // not called at the beginning of a set
            throw std::logic_error("The iterator doesn't start from a limit group.");
        else 
            it++;
        
        Container<std::pair<Limits, size_t>>::iterator answer = Node<UniqueMatchDataPtr, symbol_t>::all_limits.insert(Node<UniqueMatchDataPtr, symbol_t>::all_limits.end(), {Limits::common_edge, Node<UniqueMatchDataPtr, symbol_t>::all_limits.size()});
        bool min = true;
        size_t number = 0;

        number = 0;
        while(*it != '}') {
            if (*it == ',') {
                min = false;
                answer->first.min = number;
                number = 0;
            }
            else { // it is a digit
                number = number * 10 + (*it - '0');
            }
            it ++;
        }

        if (!min && number != 0)
            answer->first.max = number;
        if (!min && number == 0)
            answer->first.max = std::nullopt;
        if (min)
            answer->first.max = number;

        const size_t leafs = lastest.get_leafs().size();

        if (answer->first.min == 0) {
            for (auto root : parent_of_latest.get_leafs()) {
                lastest.leafs.push_back(root);
            }
            answer->first.min = 1;
        }
        answer->first.min = answer->first.min - 1;
        if (answer->first.max.has_value())
            answer->first.max = answer->first.max.value() - 1;

        if (answer->first.is_allowed_to_repeat()) {
            for (size_t i = 0 ; i < leafs ; i ++) {
                for (auto root : lastest.get_roots()) {
                    lastest.get_leafs()[i]->connect_with(root, regex, answer);
                }
            }
        }

        return answer;
    }

    template<typename UniqueMatchDataPtr, typename symbol_t>
    template<typename ConstIterator>
    SubTree<Node<UniqueMatchDataPtr, symbol_t>> RegexMatcher<UniqueMatchDataPtr, symbol_t>::processSet(std::vector<Node<UniqueMatchDataPtr, symbol_t>*> parents, UniqueMatchDataPtr regex, ConstIterator& it) {
        if (*it != '[') // not called at the beginning of a set
            throw std::logic_error("The iterator doesn't start from a set group.");
        else 
            it++;
        std::vector<Node<UniqueMatchDataPtr, symbol_t>*> leafs;
        ConstIterator prev;
        bool takeTheNextSymbolLitterally = false;
        while(*it != ']') {
            if (!takeTheNextSymbolLitterally) {
                if (*it == '\\') { // escape symbol is always followed by a reglar character
                    it ++; // so it is included no matter what
                    takeTheNextSymbolLitterally = true;
                }
                else if (*it == '-') {
                    it ++;
                    for (char ch = ((*prev) + 1) ; ch <= *it ; ch ++) {
                        Node<UniqueMatchDataPtr, symbol_t>* nextLeaf = nullptr;
                        for (auto parent : parents)
                            if (parent->hasChild(ch)) {
                                nextLeaf = parent->getChild(ch);
                                break;
                            }
                        if (nextLeaf == nullptr) {
                            nextLeaf = new Node<UniqueMatchDataPtr, symbol_t>(ch);
                        }
                        leafs.push_back(nextLeaf);
                    }
                }
                // TODO: implement not
                else {
                    takeTheNextSymbolLitterally = true;
                }
            }
            if (takeTheNextSymbolLitterally)
            {
                Node<UniqueMatchDataPtr, symbol_t>* nextLeaf = nullptr;
                for (auto parent : parents)
                    if (parent->hasChild(*it)) {
                        nextLeaf = parent->getChild(*it);
                        break;
                    }
                if (nextLeaf == nullptr) {
                    nextLeaf = new Node<UniqueMatchDataPtr, symbol_t>(*it);
                }
                leafs.push_back(nextLeaf);
                takeTheNextSymbolLitterally = false;
            }
            prev = it;
            it ++;
        }
        return {leafs, leafs};
    }

    template<typename UniqueMatchDataPtr, typename symbol_t>
    template<typename ConstIterator>
    SubTree<Node<UniqueMatchDataPtr, symbol_t>> RegexMatcher<UniqueMatchDataPtr, symbol_t>::process(std::vector<Node<UniqueMatchDataPtr, symbol_t>*> parents, UniqueMatchDataPtr regex, ConstIterator& it, ConstIterator end, const bool inBrackets) {
        SubTree<Node<UniqueMatchDataPtr, symbol_t>> answer = {{}, {}};
        std::vector<SubTree<Node<UniqueMatchDataPtr, symbol_t>>> nodeLayers = {{parents, parents}};
        for ( ; it != end ; it ++) {
            if (*it == ')' && inBrackets)
                break;
            if (*it == '[') { // start of a set
                const auto parents = nodeLayers.back();
                SubTree<Node<UniqueMatchDataPtr, symbol_t>> newNodes = processSet(parents.get_leafs(), regex, it);
                for (auto parent : parents.get_leafs()) {
                    for (auto newNode : newNodes.get_leafs()) {
                        parent->connect_with(newNode, regex);
                    }
                }
                nodeLayers.push_back(newNodes);
            } 
            else if (*it == '(') { // start of a regex in brackets
                it ++;
                SubTree<Node<UniqueMatchDataPtr, symbol_t>> newLayer = process(nodeLayers.back().get_leafs(), regex, it, end, true); // leaves it at the closing bracket
                nodeLayers.push_back(newLayer);
            }
            else if (*it == '|') {
                answer.roots.insert(answer.roots.end(), nodeLayers[1].get_leafs().begin(), nodeLayers[1].get_leafs().end());
                answer.leafs.insert(answer.leafs.end(), nodeLayers.back().get_leafs().begin(), nodeLayers.back().get_leafs().end());
                nodeLayers.resize(1);
            }
            else if (*it == '{') {
                Container<std::pair<Limits, size_t>>::const_iterator limits = processLimit(nodeLayers[nodeLayers.size() - 2], nodeLayers.back(), regex, it);
            }
            else if (auto special_regex = Node<UniqueMatchDataPtr, symbol_t>::special_symbols.find(*it); special_regex != Node<UniqueMatchDataPtr, symbol_t>::special_symbols.end()) {
                auto tmp_it = special_regex->second.cbegin();
                Container<std::pair<Limits, size_t>>::const_iterator limits = processLimit(nodeLayers[nodeLayers.size() - 2], nodeLayers.back(), regex, tmp_it);
            }
            else { // normal character
                symbol<symbol_t> sym;
                if (*it == '\\') { // skip escape symbol
                    it ++;
                    sym = symbol<symbol_t>(*it);
                }
                else if (*it == '.') 
                    sym = symbol<symbol_t>::Any;
                else
                    sym = symbol<symbol_t>(*it);
                Node<UniqueMatchDataPtr, symbol_t>* nextNode = nullptr;
                for (auto parent : nodeLayers.back().get_leafs()) {
                    if (parent->neighbours.find(sym) != parent->neighbours.end()) {
                        nextNode = parent->neighbours[sym].to;
                        break;
                    }
                }
                if (nextNode == nullptr) {
                    nextNode = new Node<UniqueMatchDataPtr, symbol_t>(sym);
                }
                for (auto parent : nodeLayers.back().get_leafs()) {
                    parent->connect_with(nextNode, regex);
                }
                nodeLayers.push_back({{nextNode}, {nextNode}});
            }
        }
        answer.roots.insert(answer.roots.end(), nodeLayers[1].get_leafs().begin(), nodeLayers[1].get_leafs().end());
        answer.leafs.insert(answer.leafs.end(), nodeLayers.back().get_leafs().begin(), nodeLayers.back().get_leafs().end());
        if (it == end) {
            Node<UniqueMatchDataPtr, symbol_t>* end_of_regex = new Node<UniqueMatchDataPtr, symbol_t>(symbol<symbol_t>::EOR);
            SubTree<Node<UniqueMatchDataPtr, symbol_t>> final_answer = {answer.get_roots(), {end_of_regex}};
            for (auto parent : answer.leafs) {
                parent->connect_with(end_of_regex, regex);
            }
            return final_answer;
        }

        return answer;
    }
}