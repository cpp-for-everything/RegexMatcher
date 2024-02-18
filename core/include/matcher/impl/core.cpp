#ifndef CORE_IMPL
#define CORE_IMPL

#include <matcher/core.hpp>

#include <list>

namespace matcher {

    template<typename RegexData, typename char_t>
    template<typename ConstIterator>
    std::list<Limits>::iterator RegexMatcher<RegexData, char_t>::processLimit(const SubTree<Node<RegexData, char_t>>& parent_of_latest, SubTree<Node<RegexData, char_t>>& lastest, RegexData regex, ConstIterator& it) {
        if (*it != '{') // not called at the beginning of a set
            throw std::logic_error("The iterator doesn't start from a limit group.");
        else 
            it++;
        
        std::list<Limits>::iterator answer = Node<RegexData, char_t>::all_limits.insert(Node<RegexData, char_t>::all_limits.end(), Limits::common_edge);
        bool min = true;
        size_t number = 0;

        number = 0;
        while(*it != '}') {
            if (*it == ',') {
                min = false;
                answer->min = number;
                number = 0;
            }
            else { // it is a digit
                number = number * 10 + (*it - '0');
            }
            it ++;
        }

        if (!min && number != 0)
            answer->max = number;
        if (!min && number == 0)
            answer->max = std::nullopt;
        if (min)
            answer->max = number;

        const size_t leafs = lastest.get_leafs().size();

        if (answer->min == 0) {
            for (auto root : parent_of_latest.get_leafs()) {
                lastest.leafs.push_back(root);
            }
            answer->min = 1;
        }
        answer->min = answer->min - 1;
        if (answer->max.has_value())
            answer->max = answer->max.value() - 1;

        if (answer->is_allowed_to_repeat()) {
            for (size_t i = 0 ; i < leafs ; i ++) {
                for (auto root : lastest.get_roots()) {
                    lastest.get_leafs()[i]->connect_with(root, regex, answer);
                }
            }
        }

        return answer;
    }

    template<typename RegexData, typename char_t>
    template<typename ConstIterator>
    SubTree<Node<RegexData, char_t>> RegexMatcher<RegexData, char_t>::processSet(std::vector<Node<RegexData, char_t>*> parents, [[maybe_unused]] RegexData regex, ConstIterator& it) {
        if (*it != '[') // not called at the beginning of a set
            throw std::logic_error("The iterator doesn't start from a set group.");
        else 
            it++;
        std::vector<Node<RegexData, char_t>*> leafs;
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
                        Node<RegexData, char_t>* nextLeaf = nullptr;
                        for (auto parent : parents)
                            if (parent->hasChild(ch)) {
                                nextLeaf = parent->getChild(ch);
                                break;
                            }
                        if (nextLeaf == nullptr) {
                            nextLeaf = new Node<RegexData, char_t>(ch);
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
                Node<RegexData, char_t>* nextLeaf = nullptr;
                for (auto parent : parents)
                    if (parent->hasChild(*it)) {
                        nextLeaf = parent->getChild(*it);
                        break;
                    }
                if (nextLeaf == nullptr) {
                    nextLeaf = new Node<RegexData, char_t>(*it);
                }
                leafs.push_back(nextLeaf);
                takeTheNextSymbolLitterally = false;
            }
            prev = it;
            it ++;
        }
        return {leafs, leafs};
    }

    template<typename RegexData, typename char_t>
    template<typename ConstIterator>
    SubTree<Node<RegexData, char_t>> RegexMatcher<RegexData, char_t>::process(std::vector<Node<RegexData, char_t>*> parents, RegexData regex, ConstIterator& it, ConstIterator end, const bool inBrackets) {
        SubTree<Node<RegexData, char_t>> answer = {{}, {}};
        std::vector<SubTree<Node<RegexData, char_t>>> nodeLayers = {{parents, parents}};
        for ( ; it != end ; it ++) {
            if (*it == ')' && inBrackets)
                break;
            if (*it == '[') { // start of a set
                const auto latest_parents = nodeLayers.back();
                SubTree<Node<RegexData, char_t>> newNodes = processSet(latest_parents.get_leafs(), regex, it);
                for (auto parent : latest_parents.get_leafs()) {
                    for (auto newNode : newNodes.get_leafs()) {
                        parent->connect_with(newNode, regex);
                    }
                }
                nodeLayers.push_back(newNodes);
            } 
            else if (*it == '(') { // start of a regex in brackets
                it ++;
                SubTree<Node<RegexData, char_t>> newLayer = process(nodeLayers.back().get_leafs(), regex, it, end, true); // leaves it at the closing bracket
                nodeLayers.push_back(newLayer);
            }
            else if (*it == '|') {
                answer.roots.insert(answer.roots.end(), nodeLayers[1].get_leafs().begin(), nodeLayers[1].get_leafs().end());
                answer.leafs.insert(answer.leafs.end(), nodeLayers.back().get_leafs().begin(), nodeLayers.back().get_leafs().end());
                nodeLayers.resize(1);
            }
            else if (*it == '{') {
                [[maybe_unused]] std::list<Limits>::iterator limits = processLimit(nodeLayers[nodeLayers.size() - 2], nodeLayers.back(), regex, it);
            }
            else if (auto special_regex = Node<RegexData, char_t>::special_symbols.find(*it); special_regex != Node<RegexData, char_t>::special_symbols.end()) {
                auto tmp_it = special_regex->second.cbegin();
                [[maybe_unused]]std::list<Limits>::iterator limits = processLimit(nodeLayers[nodeLayers.size() - 2], nodeLayers.back(), regex, tmp_it);
            }
            else { // normal character
                symbol<char_t> sym;
                if (*it == '\\') { // skip escape symbol
                    it ++;
                    sym = symbol<char_t>(*it);
                }
                else if (*it == '.') 
                    sym = symbol<char_t>::Any;
                else
                    sym = symbol<char_t>(*it);
                Node<RegexData, char_t>* nextNode = nullptr;
                for (auto parent : nodeLayers.back().get_leafs()) {
                    if (parent->neighbours.find(sym) != parent->neighbours.end()) {
                        nextNode = parent->neighbours[sym].to;
                        break;
                    }
                }
                if (nextNode == nullptr) {
                    nextNode = new Node<RegexData, char_t>(sym);
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
            Node<RegexData, char_t>* end_of_regex = new Node<RegexData, char_t>(symbol<char_t>::EOR);
            SubTree<Node<RegexData, char_t>> final_answer = {answer.get_roots(), {end_of_regex}};
            for (auto parent : answer.leafs) {
                parent->connect_with(end_of_regex, regex);
            }
            return final_answer;
        }

        return answer;
    }

    template<typename RegexData, typename char_t>
    template<typename Iterable>
    void RegexMatcher<RegexData, char_t>::add_regex(Iterable str, RegexData uid) {
        auto it = std::cbegin(str);
        process(std::vector{&root}, uid, it, std::cend(str), false);
    }

    template<typename RegexData, typename char_t>
    template<typename Iterable>
    std::vector<RegexData> RegexMatcher<RegexData, char_t>::match(Iterable str) {
        return root.match(std::cbegin(str), std::cend(str));
    }

}

#endif
