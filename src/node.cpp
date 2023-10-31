#include "../lib/node.hpp"

bool Node::hasChild(symbol ch) {
    if (this->neighbours.find(ch) != this->neighbours.end())
        return true;
}

Node* Node::getChild(symbol ch) {
    return this->neighbours.find(ch)->second.to;
}