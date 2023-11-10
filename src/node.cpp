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

std::vector<UniqueMatchDataPtr> Node::match(std::string text) {
    std::vector<UniqueMatchDataPtr> answer;
    if (this->neighbours.find(text[0]) != this->neighbours.end()) {
        std::vector<UniqueMatchDataPtr> paths;
        for (auto path : this->neighbours[text[0]].paths) {
            paths.push_back(path.first);
        }
        for (auto match : this->neighbours[text[0]].to->match_helper(text, 1, paths)) {
            answer.push_back(match);
        }
    }
    if (this->neighbours.find(symbol::Any) != this->neighbours.end()) {
        std::vector<UniqueMatchDataPtr> paths;
        for (auto path : this->neighbours[symbol::Any].paths) {
            paths.push_back(path.first);
        }
        for (auto match : this->neighbours[symbol::Any].to->match_helper(text, 1, paths)) {
            answer.push_back(match);
        }
    }
    return std::move(answer);
}

std::vector<UniqueMatchDataPtr> Node::match_helper(const std::string& text, size_t index, std::vector<UniqueMatchDataPtr> paths) {
    if (index == text.size()) {
        if (this->neighbours.find(symbol::EOR) != this->neighbours.end()) {
            return common_values(paths, this->neighbours[symbol::EOR].paths);
        }
        return {};
    }
    std::vector<UniqueMatchDataPtr> answer;
    if (this->neighbours.find(text[index]) != this->neighbours.end()) {
        for (auto match : this->neighbours[text[index]].to->match_helper(text, index + 1, common_values(paths, this->neighbours[text[index]].paths))) {
            answer.push_back(match);
        }
    }
    if (this->neighbours.find(symbol::Any) != this->neighbours.end()) {
        std::vector<UniqueMatchDataPtr> paths;
        for (auto path : this->neighbours[symbol::Any].paths) {
            paths.push_back(path.first);
        }
        for (auto match : this->neighbours[symbol::Any].to->match_helper(text, index + 1, common_values(paths, this->neighbours[text[index]].paths))) {
            answer.push_back(match);
        }
    }
    return std::move(answer);
}

std::vector<UniqueMatchDataPtr> Node::common_values(const std::vector<UniqueMatchDataPtr>& sorted, const std::map<UniqueMatchDataPtr, std::optional<std::list<Limits>::iterator>>& paths) {
    std::vector<UniqueMatchDataPtr> answer;
    auto it = paths.cbegin();
    size_t ind = 0;
    while (ind < sorted.size() && it != paths.cend()) {
        if (it->first == sorted[ind]) {
            answer.push_back(it->first);
            it ++;
            ind ++;
        } else if (it->first < sorted[ind]) {
            it ++;
        } else {
            ind ++;
        }
    }
    return std::move(answer);
}