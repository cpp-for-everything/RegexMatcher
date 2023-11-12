#include "../node.hpp"

/**
 * @brief 
 * 
 * @tparam ConstIterator 
 * @param parents  
 * @param regex 
 * @param it 
 * @return 
 */
template<typename ConstIterator>
std::list<Limits>::iterator processLimit(const SubTree& parent_of_latest, SubTree& lastest, UniqueMatchDataPtr regex, ConstIterator& it) {
    if (*it != '{') // not called at the beginning of a set
    {
        std::cout << "Boza";
        throw "pass";
    }
    else 
        it++;
    
    std::list<Limits>::iterator answer = Node::all_limits.insert(Node::all_limits.end(), Limits::common_edge);
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

    for (auto leaf : lastest.get_leafs()) {
        for (auto root : lastest.get_roots()) {
            leaf->connect_with(root, regex, answer);
            // for (auto root_child : root->neighbours) {
            //     if (root_child.second.paths.find(regex) != root_child.second.paths.end()) {
            //         leaf->connect_with(root_child.second.to, regex, answer);
            //     }
            // }
        }
    }

    if (answer->min == 0) {
        for (auto root : parent_of_latest.get_leafs()) {
            lastest.leafs.push_back(root);
        }
        answer->min = 1;
    }
    answer->min = answer->min - 1;
    if (answer->max.has_value())
        answer->max = answer->max.value() - 1;

    return answer;
}
