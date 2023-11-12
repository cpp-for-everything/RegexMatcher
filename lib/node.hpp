#include "symbol.hpp"
#include "edge_info.hpp"

#ifndef NODE_H
#define NODE_H

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

class Node;

using UniqueMatchDataPtr = int;
using EdgeInfo_t = EdgeInfo<UniqueMatchDataPtr, Node>;

class Node;

class SubTree {
public:
    std::vector<Node*> roots;
    std::vector<Node*> leafs;

    SubTree(std::vector<Node*> a, std::vector<Node*> b) : roots(a), leafs(b) {}
    SubTree() : roots(), leafs() {}

    const std::vector<Node*>& get_roots() const {
        return roots;
    }

    const std::vector<Node*>& get_leafs() const {
        return leafs;
    }
};

class Node {
private:
    static std::map<symbol, std::string> special_symbols;
    static std::list<Limits> all_limits;

    /**
     * @brief All directly connected nodes
     * 
     */
    std::map<symbol, EdgeInfo_t> neighbours;

    /**
     * @brief The current symbol that the regex would match
     * 
     */
    symbol current_symbol;

public:
    /**
     * @brief Construct a new Node object
     * 
     */
    Node () {
        current_symbol = symbol::None;
    }

    /**
     * @brief Construct a new Node object
     * 
     * @param ch Node's symbol
     */
    Node (symbol ch) {
        current_symbol = ch;
    }

private:
    /**
     * @brief Represents the current node's symbol as string
     * 
     * @return std::string string representation of the node's symbol
     */
    std::string to_string() const {
        if (current_symbol == symbol::Any)
            return "wildcard";
        if (current_symbol == symbol::None)
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
    bool hasChild(symbol ch);

    /**
     * @brief Get the Child object with the given symbol
     * 
     * @param ch the symbol that's being looked for
     * @return Node* the node correspondign to this symbol
     */
    Node* getChild(symbol ch);

    /**
     * @brief Adds a child node to the current one and marks the connection as part of a given regex match
     * 
     * @param child  Existing node
     * @param regex  Regex data that is being used to indentify the regex that the edge is part of
     * @param limits Pointer to the shared limit of the edge (nullptr if no limit is applied)
     */
    void connect_with(Node* child, UniqueMatchDataPtr regex, std::optional<std::list<Limits>::iterator> limits = std::nullopt);

public:
    /**
     * @brief Matches a string with all regexes and returns the identified of the one that matches
     * 
     * @param text string that is being tried to be matched with any of the added regexes
     * @return std::vector<UniqueMatchDataPtr> set of unique identifiers of the regexes that matches the string
     */
    std::vector<UniqueMatchDataPtr> match(std::string text);

private:

    std::vector<UniqueMatchDataPtr> match_helper(const std::string& text, size_t index, std::vector<UniqueMatchDataPtr> paths);

    template<typename T>
    static std::vector<UniqueMatchDataPtr> common_values(const std::vector<UniqueMatchDataPtr>& sorted, const std::map<UniqueMatchDataPtr, T>& paths);

    void print_helper(size_t layer, std::set<const Node*>& traversed, std::map<const Node*, std::string>& nodes) const {
        if (traversed.find(this) != traversed.end())
            return;
        const std::string layer_str = (std::stringstream() << layer).str() + "_";
        const std::string next_layer = (std::stringstream() << (layer + 1)).str() + "_";
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
public:
    void print() const {
        std::set<const Node*> traversed;
        std::map<const Node*, std::string> nodes;
        print_helper(0, traversed, nodes);
    }

    template<typename ConstIterator>
    friend SubTree process(std::vector<Node*>, UniqueMatchDataPtr, ConstIterator&, ConstIterator, const bool);

    template<typename ConstIterator>
    friend SubTree processSet(std::vector<Node*>, UniqueMatchDataPtr, ConstIterator&);
    
    template<typename ConstIterator>
    friend std::list<Limits>::iterator processLimit(const SubTree&, SubTree&, UniqueMatchDataPtr, ConstIterator&);
};

#include "processing/process_set.hpp"
#include "processing/process_limit.hpp"
#include "processing/process.hpp"

#endif
