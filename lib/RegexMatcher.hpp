#include "symbol.hpp"
#include "edge_info.hpp"
#include "subtree.hpp"
#include "node.hpp"

#ifndef RegexMatcher_H
#define RegexMatcher_H

#include <list>
#include <string>
#include <sstream>
#include <iostream>
#include <stack>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <optional>

template<typename UniqueMatchDataPtr, typename symbol_t>
class RegexMatcher {
    Node<UniqueMatchDataPtr, symbol_t> root;

    /**
     * @brief This function initialize nodes with a substring of a parameter str within the range of [start, end)
     * 
     * @tparam ConstIterator iterator for the current regex
     * @param parents List of nodes that are suposed to be right before the current one
     * @param regex Unique identifier of the regex related to the one that's being matched
     * @param it The starting iterator of the substring that is being processed
     * @param end The iterator after the last iterator that is part of the considered substring
     * @param inBrackets If the current call is for a substring of the regex, this parameter is true, otherwise false.
     * @param leafs List where the leafs would be added at the end of the call.
     * @return std::vector<Node<UniqueMatchDataPtr>*> Returns a vector of pointers to the roots of the processed substring [start, end)
     */
    template<typename ConstIterator>
    static SubTree<Node<UniqueMatchDataPtr, symbol_t>> process(std::vector<Node<UniqueMatchDataPtr, symbol_t>*>, UniqueMatchDataPtr, ConstIterator&, ConstIterator, const bool);

    template<typename ConstIterator>
    static SubTree<Node<UniqueMatchDataPtr, symbol_t>> processSet(std::vector<Node<UniqueMatchDataPtr, symbol_t>*>, UniqueMatchDataPtr, ConstIterator&);
    
    template<typename ConstIterator>
    static std::list<Limits>::iterator processLimit(const SubTree<Node<UniqueMatchDataPtr, symbol_t>>&, SubTree<Node<UniqueMatchDataPtr, symbol_t>>&, UniqueMatchDataPtr, ConstIterator&);

    std::vector<UniqueMatchDataPtr> all_paths;
public:
    /**
     * @brief Construct a new Regex Matcher object
     * 
     */
    RegexMatcher() {}

    /**
     * @brief adds another regex to the set of regexes
     * 
     * @tparam Iterable Set of characters. Must implement std::cbegin and std::cend
     */
    template<typename Iterable>
    void add_regex(Iterable str, UniqueMatchDataPtr uid) {
        all_paths.push_back(uid);
        auto it = std::cbegin(str);
        process({&root}, uid, it, std::cend(str), false);
    }

    /**
     * @brief matches a string with all added regexes
     * 
     * @tparam Iterable Set of characters. Must implement std::cbegin and std::cend
     * @return std::vector<UniqueMatchDataPtr> Set of regexes' UIDs that match the text
     */
    template<typename Iterable>
    std::vector<UniqueMatchDataPtr> match(Iterable str) {
        return root.match(std::cbegin(str), std::cend(str), all_paths);
    }

    /**
     * @brief prints the list of edges inside the matcher graph on stdout
     * 
     */
    void print_list_of_edges() const {
        root.print();
    }
};

#include "../src/RegexMatcher.cpp"

#endif
