#include "../lib/node.hpp"

template<typename UniqueMatchDataPtr, typename char_t>
std::list<Limits> Node<UniqueMatchDataPtr, char_t>::all_limits = std::list<Limits>();

template<typename UniqueMatchDataPtr, typename char_t>
std::map<symbol<char_t>, std::string> Node<UniqueMatchDataPtr, char_t>::special_symbols = {
    {'+', "{1,}"},
    {'*', "{0,}"},
    {'?', "{0,1}"}
};

template<typename UniqueMatchDataPtr, typename char_t>
bool Node<UniqueMatchDataPtr, char_t>::hasChild(symbol<char_t> ch) {
    return (this->neighbours.find(ch) != this->neighbours.end());
}

template<typename UniqueMatchDataPtr, typename char_t>
typename Node<UniqueMatchDataPtr, char_t>* Node<UniqueMatchDataPtr, char_t>::getChild(symbol<char_t> ch) {
    return this->neighbours.find(ch)->second.to;
}

template<typename UniqueMatchDataPtr, typename char_t>
void Node<UniqueMatchDataPtr, char_t>::connect_with(typename Node<UniqueMatchDataPtr, char_t>* child, UniqueMatchDataPtr regex, std::optional<std::list<Limits>::iterator> limit) {
    if (neighbours.find(child->current_symbol) != neighbours.end()) {
        neighbours[child->current_symbol].paths.emplace(regex, limit);
        return;
    }
    neighbours[child->current_symbol].paths.emplace(regex, limit);
    neighbours[child->current_symbol].to = child;
}

template<typename UniqueMatchDataPtr, typename char_t>
template<typename ConstIterator>
std::vector<UniqueMatchDataPtr> Node<UniqueMatchDataPtr, char_t>::match(ConstIterator begin, ConstIterator end) {
    if (begin == end) {
        return {};
    }
    std::vector<UniqueMatchDataPtr> answer;
    for (symbol<char_t> ch : {symbol<char_t>(*begin), symbol<char_t>::Any, symbol<char_t>::None}) {
        if (this->neighbours.find(ch) != this->neighbours.end()) {
            std::vector<UniqueMatchDataPtr> paths;
            for (auto path : this->neighbours[ch].paths) {
                paths.push_back(path.first);
            }
            if (ch != symbol<char_t>::None) begin++;
            for (auto match : this->neighbours[ch].to->match_helper(begin, end, paths)) {
                answer.push_back(match);
            }
            if (ch != symbol<char_t>::None) begin--;
        }
    }
    return std::move(answer);
}

template<typename UniqueMatchDataPtr, typename char_t>
template<typename ConstIterator>
std::vector<UniqueMatchDataPtr> Node<UniqueMatchDataPtr, char_t>::match_helper(ConstIterator begin, ConstIterator end, std::vector<UniqueMatchDataPtr> paths) {
    if (paths.size() == 0) return {};
    if (begin == end) {        
        if (auto it = this->neighbours.find(symbol<char_t>::EOR); it != this->neighbours.end()) {
            return common_values(paths, it->second.paths);
        }
        return {};
    }
    std::vector<UniqueMatchDataPtr> answer;
    for (symbol<char_t> to_test : {symbol<char_t>(*begin), symbol<char_t>::Any, symbol<char_t>::None}) {
        if (auto it = this->neighbours.find(to_test); it != this->neighbours.end()) {
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
            if (to_test != symbol<char_t>::None) begin ++;
            for (auto match : it->second.to->match_helper(begin, end, common_values(paths, current_paths))) {
                answer.push_back(match);
            }
            if (to_test != symbol<char_t>::None) begin --;
            for (auto x : current_paths) {
                if(x.second.has_value())
                    (*it->second.paths[x.first].value()) = x.second.value();
            }
        }
    }
    return std::move(answer);
}

template<typename UniqueMatchDataPtr, typename char_t>
template<typename T>
std::vector<UniqueMatchDataPtr> Node<UniqueMatchDataPtr, char_t>::common_values(const std::vector<UniqueMatchDataPtr>& sorted, const std::map<UniqueMatchDataPtr, T>& paths) {
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
