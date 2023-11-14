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
    if (auto existing_child = neighbours.find(child->current_symbol); existing_child != neighbours.end()) {
        if (auto it = existing_child->second.paths.find(regex); it != existing_child->second.paths.end())
        {
            if (!it->second.has_value() && limit == std::nullopt) {
                it->second = Node<UniqueMatchDataPtr, char_t>::all_limits.insert(Node<UniqueMatchDataPtr, char_t>::all_limits.end(), Limits(1,1));
            }
            else if (it->second.has_value() && limit == std::nullopt) {
                (it->second.value()->min) ++;
                if (it->second.has_value()) {
                    (it->second.value()->max.value()) ++;
                }
            }
        }
        else if (this == child && limit == std::nullopt) { 
            neighbours[child->current_symbol].paths.emplace(regex, Node<UniqueMatchDataPtr, char_t>::all_limits.insert(Node<UniqueMatchDataPtr, char_t>::all_limits.end(), Limits(1,1)));
        }
        else
            neighbours[child->current_symbol].paths.emplace(regex, limit);
        return;
    }
    neighbours[child->current_symbol].paths.emplace(regex, limit);
    neighbours[child->current_symbol].to = child;
}

template<typename UniqueMatchDataPtr, typename char_t>
template<typename ConstIterator>
std::vector<UniqueMatchDataPtr> Node<UniqueMatchDataPtr, char_t>::match(ConstIterator begin, ConstIterator end, std::vector<UniqueMatchDataPtr> paths) {
    return match_helper(begin, end, paths, nullptr);
}

template<typename UniqueMatchDataPtr, typename char_t>
template<typename ConstIterator>
std::vector<UniqueMatchDataPtr> Node<UniqueMatchDataPtr, char_t>::match_helper(ConstIterator begin, ConstIterator end, std::vector<UniqueMatchDataPtr> paths, Node* prev) {
    if (paths.size() == 0) return {};
    if (begin == end) {
        if (auto it = this->neighbours.find(symbol<char_t>::EOR); it != this->neighbours.end()) {
            std::vector<UniqueMatchDataPtr> answer;
            std::vector<UniqueMatchDataPtr> potential_answer = common_values(paths, it->second.paths);
            if (prev != nullptr)
            {
                for (UniqueMatchDataPtr pathId : potential_answer) {
                    bool to_include = true;
                    auto x = prev->neighbours[this->current_symbol].paths[pathId];
                    if (x.has_value()) {
                        to_include &= x.value()->min == 0;
                    }
                    if (auto knot = this->neighbours.find(this->current_symbol); knot != this->neighbours.end()) {
                        if (knot->second.paths[pathId].has_value())
                            to_include &= knot->second.paths[pathId].value()->min == 0;
                    }
                    if (to_include) {
                        answer.push_back(pathId);
                    }
                }
                return std::move(answer);
            }
            else 
                return std::move(potential_answer);
        }
        return {};
    }
    std::vector<UniqueMatchDataPtr> answer;
    const symbol<char_t> current = symbol<char_t>(*begin);
    for (symbol<char_t> to_test : {current, symbol<char_t>::Any, symbol<char_t>::None}) {
        if (auto it = this->neighbours.find(to_test); it != this->neighbours.end()) {
            std::map<UniqueMatchDataPtr, std::optional<Limits>> current_paths;
            for (auto path : it->second.paths) {
                if (path.second.has_value() && !path.second.value()->is_allowed_to_repeat()) continue;
                if (path.second.has_value()) {
                    current_paths.emplace(path.first, *path.second.value());
                    --(*path.second.value());
                }
                else {
                    current_paths.emplace(path.first, std::nullopt);
                }
            }
            if (to_test != symbol<char_t>::None) begin ++;
            for (auto match : it->second.to->match_helper(begin, end, common_values(paths, current_paths), this)) {
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
