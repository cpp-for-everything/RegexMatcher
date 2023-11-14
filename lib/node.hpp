#ifndef NODE_H
#define NODE_H

#include "symbol.hpp"
#include "edge_info.hpp"
#include "subtree.hpp"

#include <set>

template<typename UniqueMatchDataPtr, typename symbol_t>
class RegexMatcher;

template<typename UniqueMatchDataPtr, typename char_t>
class Node {
private:
    static std::map<symbol<char_t>, std::string> special_symbols;
    static std::list<Limits> all_limits;

    /**
     * @brief All directly connected nodes
     * 
     */
    std::map<symbol<char_t>, EdgeInfo<UniqueMatchDataPtr, Node<UniqueMatchDataPtr, char_t>>> neighbours;

    /**
     * @brief The current symbol that the regex would match
     * 
     */
    symbol<char_t> current_symbol;

    /**
     * @brief Construct a new Node object
     * 
     */
    Node () {
        current_symbol = symbol<char_t>::None;
    }

    /**
     * @brief Construct a new Node object
     * 
     * @param ch Node's symbol<char_t>
     */
    Node (symbol<char_t> ch) {
        current_symbol = ch;
    }

    /**
     * @brief Represents the current node's symbol as string
     * 
     * @return std::string string representation of the node's symbol<char_t>
     */
    std::string to_string() const {
        if (current_symbol == symbol<char_t>::Any)
            return "wildcard";
        if (current_symbol == symbol<char_t>::None)
            return "(empty)";
        return std::string(1, current_symbol.ch);
    }

    /**
     * @brief Checks if there is a node with a given symbol in the neighbour list  
     * 
     * @param ch Symbol to be checked
     * @return true if a node with this symbol is direct neighbour to this node
     * @return false if there is no node with this symbol as direct neighbour to this node
     */
    bool hasChild(symbol<char_t> ch);

    /**
     * @brief Get the Child object with the given symbol
     * 
     * @param ch the symbol that's being looked for
     * @return Node* the node correspondign to this symbol
     */
    Node<UniqueMatchDataPtr, char_t>* getChild(symbol<char_t> ch);

    /**
     * @brief Adds a child node to the current one and marks the connection as part of a given regex match
     * 
     * @param child  Existing node
     * @param regex  Regex data that is being used to indentify the regex that the edge is part of
     * @param limits Pointer to the shared limit of the edge (nullptr if no limit is applied)
     */
    void connect_with(Node<UniqueMatchDataPtr, char_t>* child, UniqueMatchDataPtr regex, std::optional<std::list<Limits>::iterator> limits = std::nullopt);

    /**
     * @brief Matches a string with all regexes and returns the identified of the one that matches
     * 
     * @param text string that is being tried to be matched with any of the added regexes
     * @tparam ConstIterator const iterator in a container
     * @return std::vector<UniqueMatchDataPtr> set of unique identifiers of the regexes that matches the string
     */
    template<typename ConstIterator>
    std::vector<UniqueMatchDataPtr> match(ConstIterator, ConstIterator);

    template<typename ConstIterator>
    std::vector<UniqueMatchDataPtr> match_helper(ConstIterator, ConstIterator, const std::vector<UniqueMatchDataPtr>&, Node*);

    template<typename T>
    static std::vector<UniqueMatchDataPtr> common_values(const std::vector<UniqueMatchDataPtr>& sorted, const std::map<UniqueMatchDataPtr, T>& paths);

    void print_helper(size_t layer, std::set<const Node<UniqueMatchDataPtr, char_t>*>& traversed, std::map<const Node<UniqueMatchDataPtr, char_t>*, std::string>& nodes) const {
        if (traversed.find(this) != traversed.end())
            return;
        const std::basic_string<char_t> layer_str = (std::basic_stringstream<char_t>() << layer).str() + "_";
        const std::basic_string<char_t> next_layer = (std::basic_stringstream<char_t>() << (layer + 1)).str() + "_";
        traversed.emplace(this);
        nodes.emplace(this, layer_str + current_symbol.to_string());
        for (auto child : neighbours) {
            if (nodes.find(child.second.to) == nodes.end()) {
                nodes.emplace(child.second.to, next_layer + child.second.to->current_symbol.to_string());
            }
            std::cout << nodes[this] << " " << nodes[child.second.to] << " ";
            std::cout << child.second.paths.begin()->first << Limits::to_string(child.second.paths.begin()->second);
            for (auto it = std::next(child.second.paths.begin()) ; it != child.second.paths.end() ; it ++) {
                std::cout << "," << it->first << Limits::to_string(it->second);
            }
            std::cout << std::endl;
            if (nodes.find(child.second.to) != nodes.end()) {
                child.second.to->print_helper(layer + 1, traversed, nodes);
            }
        }
    }
    
    void print() const {
        std::set<const Node<UniqueMatchDataPtr, char_t>*> traversed;
        std::map<const Node<UniqueMatchDataPtr, char_t>*, std::string> nodes;
        print_helper(0, traversed, nodes);
    }

    friend class RegexMatcher<UniqueMatchDataPtr, char_t>;
};

#include "../src/node.cpp"

#endif