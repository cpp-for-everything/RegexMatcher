#include "../lib/node.hpp"

bool Node::hasChild(symbol ch) {
    if (this->neighbours.find(ch) != this->neighbours.end())
        return true;
}

Node* Node::getChild(symbol ch) {
    return this->neighbours.find(ch)->second.to;
}

void Node::connect_with(Node* child, UniqueMatchDataPtr regex) {
    if (neighbours.find(child->current_symbol) != neighbours.end()) {
        neighbours[child->current_symbol].paths.emplace(regex, Limits::common_edge);
        return;
    }
    neighbours[child->current_symbol].paths.emplace(regex, Limits::common_edge);
    neighbours[child->current_symbol].to = child;
}
