#include "symbol.hpp"
#include "edge_info.hpp"

#ifndef NODE_H
#define NODE_H

#include <list>
#include <string>
#include <stack>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <optional>

class Node;

using UniqueMatchDataPtr = int;
using EdgeInfo_t = EdgeInfo<UniqueMatchDataPtr, Node>;

class Node {
public:
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
    Node (char ch) {
        current_symbol = symbol(ch);
    }

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
};

#include "processing/process.hpp"
#include "processing/process_set.hpp"    

#endif