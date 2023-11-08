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
SubTree process(std::vector<Node*> parents, UniqueMatchDataPtr regex, ConstIterator& it, ConstIterator end, const bool inBrackets) {
    SubTree answer = {parents, {}};
    std::vector<SubTree> nodeLayers = {{parents, parents}};
    for ( ; it != end ; it ++) {
        if (*it == ')' && inBrackets)
            break;
        if (*it == '[') { // start of a set
            const auto parents = nodeLayers.back();
            SubTree newNodes = processSet(parents.get_leafs(), regex, it);
            for (auto parent : parents.get_leafs()) {
                for (auto newNode : newNodes.get_leafs()) {
                    parent->connect_with(newNode, regex);
                }
            }
            nodeLayers.push_back(newNodes);
        } 
        else if (*it == '(') { // start of a regex in brackets
            it ++;
            SubTree newLayer = process(nodeLayers.back().get_leafs(), regex, it, end, true); // leaves it at the closing bracket
            nodeLayers.push_back(newLayer);
        }
        else if (*it == '|') {
            answer.leafs.insert(answer.leafs.end(), nodeLayers.back().get_leafs().begin(), nodeLayers.back().get_leafs().end());
            nodeLayers.resize(1);
        }
        else if (*it == '{') {
            std::vector<Limits>::iterator limits = processLimit(nodeLayers.back(), regex, it);
        }
        else { // normal character
            if (*it == '\\') { // skip escape symbol
                it ++;
            }
            Node* nextNode = nullptr;
            for (auto parent : nodeLayers.back().get_leafs()) {
                if (parent->neighbours.find(*it) != parent->neighbours.end()) {
                    nextNode = parent->neighbours[*it].to;
                    break;
                }
            }
            if (nextNode == nullptr) {
                nextNode = new Node(*it);
            }
            for (auto parent : nodeLayers.back().get_leafs()) {
                parent->connect_with(nextNode, regex);
            }
            nodeLayers.push_back({nodeLayers.back().get_leafs(), {nextNode}});
        }
    }
    answer.leafs.insert(answer.leafs.end(), nodeLayers.back().get_leafs().begin(), nodeLayers.back().get_leafs().end());
    return answer;
}