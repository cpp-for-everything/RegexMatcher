#include <list>
#include <string>
#include <stack>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <optional>

#include "symbol.hpp"
#include "edge_info.hpp"

class Node;

using UniqueMatchDataPtr = int;
using EdgeInfo_t = EdgeInfo<UniqueMatchDataPtr, Node>;

class Node {
private:
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
     * @brief Get the or create object
     * 
     * @param ch symbol that would lead to the new or existing node
     * @param regex regex id
     * @param limits optionally sets limits on how many times the edge can be repeated by each regex
     * @return Node* return the end node of the edge
     */
    Node* get_or_create(symbol ch, UniqueMatchDataPtr regex, std::optional<Limits> limits = std::nullopt) {
        if (auto it = neighbours.find(ch); it != neighbours.end()) {
            it->second.paths.insert({regex, limits});
            return it->second.to;
        }
        if (auto it = neighbours.find(symbol::None); it != neighbours.end()) {
            return it->second.to->get_or_create(ch, regex, limits);
        }
        return this->add_child(ch, new Node(), regex, limits);;
    }

    /**
     * @brief Creates a new node and attaches it via the symbol to the current node
     * 
     * @param ch symbol that would lead to the new node
     * @param endpoint address of the new node
     * @param regex regex id
     * @param limits optionally sets limits on how many times the edge can be repeated by each regex
     * @return Node* return the new node of the edge
     */
    Node* add_child(symbol ch, Node* endpoint, UniqueMatchDataPtr regex, std::optional<Limits> limits = std::nullopt) {
        EdgeInfo_t edge_info;
        edge_info.paths = {{regex, limits}};
        edge_info.to = endpoint;
        this->neighbours.insert({ch, edge_info});
        return endpoint;
    }
public:
    /**
     * @brief This function initialize a node with a substring of a parameter str within the range of [start, end)
     * 
     * @param start The starting iterator of the substring that is being processed
     * @param end The iterator after the last iterator that is part of the considered substring
     * @param top_level 
     * @return std::vector<Node*> Returns a vector of pointers to the leafs of the processed substring [start, end)
     */
    template<typename ConstIterator>
    std::vector<Node*> process(UniqueMatchDataPtr regex, ConstIterator& it, ConstIterator end, const bool in_brackets = false);
    
    /**
     * @brief 
     * 
     * @tparam ConstIterator 
     * @param regex 
     * @param it 
     * @return std::vector<Node*> 
     */
    template<typename ConstIterator>
    std::vector<Node*> processSet(UniqueMatchDataPtr regex, ConstIterator& it);
};