#include "../node.hpp"

/**
 * @brief 
 * 
 * @tparam ConstIterator 
 * @param parents  
 * @param regex 
 * @param it 
 * @return std::vector<Node*> 
 */
template<typename ConstIterator>
SubTree processSet(std::vector<Node*> parents, UniqueMatchDataPtr regex, ConstIterator& it) {
    if (*it != '[') // not called at the beginning of a set
        return {parents, parents};
    else it++;
    std::vector<Node*> leafs;
    ConstIterator prev;
    bool takeTheNextSymbolLitterally = false;
    while(*it != ']') {
        if (!takeTheNextSymbolLitterally) {
            if (*it == '\\') { // escape symbol is always followed by a reglar character
                it ++; // so it is included no matter what
                takeTheNextSymbolLitterally = true;
            }
            else if (*it == '-') {
                it ++;
                for (char ch = ((*prev) + 1) ; ch <= *it ; ch ++) {
                    Node* nextLeaf = nullptr;
                    for (auto parent : parents)
                        if (parent->hasChild(ch)) {
                            nextLeaf = parent->getChild(ch);
                            break;
                        }
                    if (nextLeaf == nullptr) {
                        nextLeaf = new Node(ch);
                    }
                    leafs.push_back(nextLeaf);
                }
            }
            // TODO: implement not
            else {
                takeTheNextSymbolLitterally = true;
            }
        }
        if (takeTheNextSymbolLitterally)
        {
            Node* nextLeaf = nullptr;
            for (auto parent : parents)
                if (parent->hasChild(*it)) {
                    nextLeaf = parent->getChild(*it);
                    break;
                }
            if (nextLeaf == nullptr) {
                nextLeaf = new Node(*it);
            }
            leafs.push_back(nextLeaf);
            takeTheNextSymbolLitterally = false;
        }
        prev = it;
        it ++;
    }
    return {leafs, leafs};
}
