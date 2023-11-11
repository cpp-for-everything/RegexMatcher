#include "../lib/node.hpp"

std::list<Limits> Node::all_limits = std::list<Limits>();
std::map<symbol, std::string> Node::special_symbols = {
    {'+', "{1,}"},
    {'*', "{0,}"},
    {'?', "{0,1}"}
};

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
    if (paths.size() == 0) return {};
    // std::cout << std::string(3 * index, ' ') << this->current_symbol.to_string() << " -> ";
    // if (index == text.size()) std::cout << "EOR";
    // else std::cout << text[index];
    // std::cout << " | Children: ";
    // for (auto x : this->neighbours) 
    // {
    //     if (x.second.to != nullptr)
    //         std::cout << x.second.to->current_symbol.to_string() << "(";
    //         for (auto y : x.second.paths) std::cout << y.first << " ";
    //         std::cout << "),";
    // }
    // std::cout << " | Regex candidates (UIDs): ";
    // for (auto x : paths) std::cout << x << " ";
    // std::cout << std::endl;
    if (index == text.size()) {        
        if (auto it = this->neighbours.find(symbol::EOR); it != this->neighbours.end()) {
            return common_values(paths, it->second.paths);
        }
        return {};
    }
    std::vector<UniqueMatchDataPtr> answer;
    for (symbol to_test : {symbol(text[index]), symbol::Any, symbol::None}) {
        if (auto it = this->neighbours.find(to_test); it != this->neighbours.end()) {
            //std::cout << std::string(3 * index, ' ') << "- " << "Child " << to_test.to_string() << " found:" << std::endl;
            std::map<UniqueMatchDataPtr, std::optional<Limits>> current_paths;
            for (auto path : it->second.paths) {
                if (path.second.has_value() && !path.second.value()->is_allowed_to_repeat()) continue;
                if (path.second.has_value()) {
                    current_paths.emplace(path.first, *path.second.value());
                    --(*path.second.value());
                }
                else
                    current_paths.emplace(path.first, std::nullopt);
            }
            for (auto match : it->second.to->match_helper(text, index + (to_test != symbol::None), common_values(paths, current_paths))) {
                answer.push_back(match);
            }
            for (auto x : current_paths) {
                if(x.second.has_value())
                    (*it->second.paths[x.first].value()) = x.second.value();
            }
        }
    }
    return std::move(answer);
}

template<typename T>
std::vector<UniqueMatchDataPtr> Node::common_values(const std::vector<UniqueMatchDataPtr>& sorted, const std::map<UniqueMatchDataPtr, T>& paths) {
    // std::cout << "matching: ";
    // for (auto x : sorted) std::cout << x << " ";
    // std::cout << " | ";
    // for (auto x : paths) std::cout << x.first << " ";
    // std::cout << std::endl;
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
    return answer;
}