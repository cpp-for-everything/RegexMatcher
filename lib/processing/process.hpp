#include "../node.hpp"

template<typename ConstIterator>
std::vector<Node*> Node::process(UniqueMatchDataPtr regex, ConstIterator& it, ConstIterator end, const bool in_brackets) {
    std::queue<std::vector<Node*>> queue;
    queue.push({this});
}