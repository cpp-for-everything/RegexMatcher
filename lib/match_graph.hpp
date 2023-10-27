#pragma once

#ifdef DEBUG
#include <iostream>
#endif

#include <list>
#include <string>
#include <stack>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <optional>

namespace Regex {
    template<typename MatchData, typename StringT = std::string>
    class Matcher {
    private:
        std::list<MatchData> regexes;
    public:
        using MatchDataPtr = std::list<MatchData>::iterator;
        using char_t = decltype(*std::begin(std::declval<StringT>()));

        Matcher& add (StringT, const MatchData&&);
    };

    struct Limits {
        size_t min;
        std::optional<size_t> max;
        
        Limits() : Limits (0, std::nullopt) {}
        
        Limits(size_t min, std::nullopt_t) {
            this->min = min;
            this->max = std::nullopt;
        }
        
        Limits(std::nullopt_t, size_t max) : Limits (0, max) {}
        
        Limits(size_t min, size_t max) {
            this->min = min;
            this->max = max;
        }

        static const Limits common_edge;
        static const Limits zero_or_once;
        static const Limits once_or_more;
        static const Limits zero_or_more;
    };

    const Limits common_edge = Limits();
    const Limits zero_or_once = Limits(0, 1);
    const Limits once_or_more = Limits(1, std::nullopt);
    const Limits zero_or_more = Limits(0, std::nullopt);

    /**
     * @brief Class containing the list of regexes using the given edge
     * 
     * @tparam T type of the reference to the match data
     */
    template<typename T>
    struct EdgeInfo {
        std::map<T, std::optional<Limits>> paths; // each path may have different requirements for how many times should the edge be repeated.
        Node* to;
    };

    template<typename UniqueMatchDataPtr>
    class Node {
    private:
        /**
         * @brief All directly connected nodes
         * 
         */
        std::map<char, EdgeInfo<UniqueMatchDataPtr>> children;
    public:
        /**
         * @brief This function initialize a node with a substring of a parameter str within the range of [start, end)
         * 
         * @param start The starting iterator of the substring that is being processed
         * @param end The iterator after the last iterator that is part of the considered substring
         * @param top_level 
         * @return Node* Returns a pointer to the leaf of the processed substring [start, end)
         */
        template<typename ConstIterator>
        Node* process(UniqueMatchDataPtr regex, ConstIterator& it, ConstIterator end, const bool in_brackets = false) {
            std::deque<std::pair<Node*, Node*>> nodes; // contains the root and the leaf of each sub-node
            nodes.push_back({this, nullptr});
            for ( ; it != end ; it ++) {
                if (*it == '[') { // start of a set
                    Node* setGraph = new Node();
                    setGraph->processSet(regex, it);
                    nodes.push_back(setGraph);
                }
                else if (*it == '(') { // start of a regex in brackets
                    brackets_depth ++;
                    it ++;
                    Node* subregex = new Node();
                    subregex->process(regex, it, end, true); // leaves it at the closing bracket
                    nodes.push_back(subregex);
                }
                else if (*it == ')') { // end of a regex in brackets so return time
                    if (in_brackets)
                        break;
                    else // invalid regex
                        throw InvalidRegex("Closing braket with no closing one.");
                }
                else if (*it == '+') { // repeat last node 1+ times
                    std::pair<Node*, Node*> sub_node = nodes.back();
                    Node* const root = sub_node->first;
                    Node* const leaf = sub_node->second;
                    for (auto child : root->children) {
                        leaf->add_child(child.first, child.second.to, child.second.paths[regex].limits, regex);
                    }
                }
                else if (*it == '\\') {
                    it ++;
                    Node* newChild = new Node();
                    newNode->add_child(*it, new Node());
                }
            }
        }

        template<typename ConstIterator>
        Node* processSet(UniqueMatchDataPtr regex, ConstIterator& it) {
            if (*it != '[') // not called at the beginning of a set
                return nullptr;
            Node* leaf = new Node();
            ConstIterator prev;
            while(*it != ']') {
                if (*it == '\\') { // escape symbol is always followed by a reglar character
                    it ++; // so it is included no matter what
                }
                else if (*it == '-') {
                    it ++;
                    for (char ch = ((*prev) + 1) ; ch <= *it ; ch ++) {
                        top_level->add_child(ch, leaf, regex, Limits::common_edge);
                    }
                }
                prev = it;
                it ++;
            }
            return leaf;
        }

        Node* add_child(char ch, Node* to, std::optional<Limits> limits, UniqueMatchDataPtr regex_ptr) {
            if (children.find(ch) != children.end()) {
                if (children[ch].to != to) throw "The \"to\" Node pointers should get synced";
                children[ch].to = to;
                children[ch].paths.insert({regex_ptr, limits});
            }
            else {
                throw "Merge not implemented";
            }
            return this;
        }
    };
}

/*
(a|b)+                     -> a b | +
a|b+                       -> a b + |
*/