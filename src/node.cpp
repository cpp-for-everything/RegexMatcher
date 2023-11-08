#include "../lib/node.hpp"

std::list<Limits> Node::all_limits = std::list<Limits>();

bool Node::hasChild(symbol ch) {
    if (this->neighbours.find(ch) != this->neighbours.end())
        return true;
}

Node* Node::getChild(symbol ch) {
    return this->neighbours.find(ch)->second.to;
}

void Node::connect_with(Node* child, UniqueMatchDataPtr regex, std::optional<std::list<Limits>::iterator> limit) {
    if (neighbours.find(child->current_symbol) != neighbours.end()) {
        neighbours[child->current_symbol].paths.emplace(regex, limit);
        return;
    }
    neighbours[child->current_symbol].paths.emplace(regex, limit);
    neighbours[child->current_symbol].to = child;
}

std::optional<UniqueMatchDataPtr> Node::match(std::string text) {
    return match_helper(text, 0);
}

std::optional<UniqueMatchDataPtr> Node::match_helper(std::string& text, size_t index) {
    return std::nullopt;
}