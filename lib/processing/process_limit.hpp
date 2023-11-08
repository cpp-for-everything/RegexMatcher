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
static std::vector<Limits>::iterator processLimit(SubTree lastest, UniqueMatchDataPtr regex, ConstIterator& it) {
    if (*it != '{') // not called at the beginning of a set
    {
        std::cout << "Boza";
        throw "pass";
    }
    else 
        it++;
    Node::all_limits.push_back(Limits::common_edge);
    std::vector<Limits>::iterator answer = Node::all_limits.end();
    answer--;
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
        }
    }

    return answer;
}
