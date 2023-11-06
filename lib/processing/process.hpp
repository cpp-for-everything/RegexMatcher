#include "../node.hpp"

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
 * @return std::vector<Node*> Returns a vector of pointers to the roots of the processed substring [start, end)
 */
template<typename ConstIterator>
std::vector<Node*> process(std::vector<Node*> parents, UniqueMatchDataPtr regex, ConstIterator& it, ConstIterator end, const bool inBrackets, std::vector<Node*>& leafs) {
    std::vector<std::vector<Node*>> nodeLayers = {parents};
    for ( ; it != end ; it ++) {
        if (*it == ')' && inBrackets)
            break;
        if (*it == '[') { // start of a set
            const auto parents = nodeLayers.back();
            const std::vector<Node*> newNodes = processSet(parents, regex, it);
            for (auto parent : parents) {
                for (auto newNode : newNodes) {
                    parent->connect_with(newNode, regex);
                }
            }
            nodeLayers.push_back(newNodes);
        } 
        else if (*it == '(') { // start of a regex in brackets
            it ++;
            std::vector<Node*> newLayersLeafs;
            const std::vector<Node*> newLayer = process(nodeLayers.back(), regex, it, end, true, newLayersLeafs); // leaves it at the closing bracket
            nodeLayers.push_back(newLayer);
            nodeLayers.push_back(newLayersLeafs);
        }
        else if (*it == '|') {
            
        }
        else if (*it == '{') {
            
        }
        else { // normal character
            if (*it == '\\') { // skip escape symbol
                it ++;
            }
            Node* nextNode = nullptr;
            for (auto parent : nodeLayers.back()) {
                if (parent->neighbours.find(*it) != parent->neighbours.end()) {
                    nextNode = parent->neighbours[*it].to;
                    break;
                }
            }
            if (nextNode == nullptr) {
                nextNode = new Node(*it);
            }
            for (auto parent : nodeLayers.back()) {
                parent->connect_with(nextNode, regex);
            }
            nodeLayers.push_back({nextNode});
        }
    }
    if (nodeLayers.size() >= 2) {
        leafs = nodeLayers.back();
        return nodeLayers[1];
    }
    else
        return {};
}