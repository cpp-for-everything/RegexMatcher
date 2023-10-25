#pragma once

#include <list>
#include <string>
#include <stack>
#include <vector>
#include <map>

namespace Regex {
    template<typename MatchData, typename StringT = std::string>
    class Matcher {
    private:
        std::list<MatchData> regexes;
    public:
        using char_t = decltype(*std::begin(std::declval<StringT>()));

        Matcher& add (StringT, const MatchData&&);
    };

    class Node {
    private:
        /**
         * @brief All directly connected nodes
         * 
         */
        std::map<char, Node*> children;

        /**
         * @brief This function initialize a node with a substring of a parameter str within the range of [start, end)
         * 
         * @param start The starting iterator of the substring that is being processed
         * @param end The iterator after the last iterator that is part of the considered substring
         * @param top_level 
         * @return Node* Returns a pointer to the leaf of the processed substring
         */
        template<typename ConstIterator>
        Node* process(ConstIterator start, ConstIterator end, Node* top_level = this) { // [start, end)
            std::stack<char> st;
            std::stack<Node*> children;
            for (ConstIterator it = start ; it != end ; it ++) {
                if (*it == '[') { // begining of a set
                    ConstIterator subprocess_start = it;
                    subprocess_start ++;
                    st.push('[');
                    while(*it != ']') {
                        if (*it == '\\') { // escape symbol is always followed by a reglar character
                            it ++; // so it is included no matter what
                        }
                        it ++;
                    }
                    Node* setGraph = new Node();
                    children.push(setGraph);
                    setGraph->processSet(subprocess_start, it);
                }
                else if (*it == '(') {

                }

            }
        }

        template<typename ConstIterator>
        Node* processSet(ConstIterator& it, Node* top_level = this) {
            Node* leaf = new Node();
            ConstIterator prev;
            while(*it != ']') {
                if (*it == '\\') { // escape symbol is always followed by a reglar character
                    it ++; // so it is included no matter what
                }
                else if (*it == '-') {
                    it ++;
                    for (char ch = ((*prev) + 1) ; ch <= *it ; ch ++) {
                        top_level->add_child(ch, leaf);
                    }
                }
                prev = it;
                it ++;
            }
            return leaf;
        }

        Node* add_child(char ch, Node* child) {
            if (this->children.find(ch) != this->children.end())
                this->children.emplace(ch, child);
            else {
                // TODO: implement merge
            }
            return this;
        }
    };
}

/*
(a|b)+                     -> a b | +
a|b+                       -> a b + |
*/