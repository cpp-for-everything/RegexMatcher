#include "../node.hpp"

template<typename ConstIterator>
std::vector<Node*> Node::processSet(UniqueMatchDataPtr regex, ConstIterator& it) {
    if (*it != '[') // not called at the beginning of a set
        return nullptr;
    std::vector<Node*> leafs;
    ConstIterator prev;
    while(*it != ']') {
        if (*it == '\\') { // escape symbol is always followed by a reglar character
            it ++; // so it is included no matter what
        }
        else if (*it == '-') {
            it ++;
            for (char ch = ((*prev) + 1) ; ch <= *it ; ch ++) {
                leaf.push_back(this->get_or_create(ch, regex, std::nullopt));
            }
        }
        else {
            leaf.push_back(this->get_or_create(*it, regex, std::nullopt));
        }
        prev = it;
        it ++;
    }
    return leafs;
}