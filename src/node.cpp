#include "../lib/node.hpp"

namespace URLMatcher {

    template<typename UniqueMatchDataPtr, typename char_t>
    Container<std::pair<Limits, size_t>> Node<UniqueMatchDataPtr, char_t>::all_limits = {};

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
    void Node<UniqueMatchDataPtr, char_t>::connect_with(typename Node<UniqueMatchDataPtr, char_t>* child, UniqueMatchDataPtr regex, std::optional<Container<std::pair<Limits, size_t>>::iterator> limit) {
        if (auto existing_child = neighbours.find(child->current_symbol); existing_child != neighbours.end()) {
            if (auto it = existing_child->second.paths.find(regex); it != existing_child->second.paths.end())
            {
                if (!it->second.has_value() && limit == std::nullopt) {
                    it->second = Node<UniqueMatchDataPtr, char_t>::all_limits.insert(Node<UniqueMatchDataPtr, char_t>::all_limits.end(), {Limits(1,1), Node<UniqueMatchDataPtr, char_t>::all_limits.size()});
                }
                else if (it->second.has_value() && limit == std::nullopt) {
                    (it->second.value()->first.min) ++;
                    if (it->second.has_value()) {
                        (it->second.value()->first.max.value()) ++;
                    }
                }
            }
            else if (this == child && limit == std::nullopt) { 
                neighbours[child->current_symbol].paths.emplace(regex, Node<UniqueMatchDataPtr, char_t>::all_limits.insert(Node<UniqueMatchDataPtr, char_t>::all_limits.end(), {Limits(1,1), Node<UniqueMatchDataPtr, char_t>::all_limits.size()}));
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
    std::vector<UniqueMatchDataPtr> Node<UniqueMatchDataPtr, char_t>::match(ConstIterator begin, ConstIterator end) const {
        Map<std::pair<Limits, size_t>> changed_limits;
        return match_helper(begin, end, {}, nullptr, changed_limits);
    }

    template<typename UniqueMatchDataPtr, typename char_t>
    template<typename ConstIterator>
    std::vector<UniqueMatchDataPtr> Node<UniqueMatchDataPtr, char_t>::match_helper(ConstIterator begin, ConstIterator end, const std::vector<UniqueMatchDataPtr>& paths, const Node* prev, Map<std::pair<Limits, size_t>>& changed_limits) const {
        //std::cout << "Haha" << std::endl;
        if (begin == end) {
            //std::cout << "End" << std::endl;
            if (auto it = this->neighbours.find(symbol<char_t>::EOR); it != this->neighbours.end()) {
                std::vector<UniqueMatchDataPtr> answer;
                std::vector<UniqueMatchDataPtr> potential_answer = common_values(paths, it->second.paths);
                //std::cout << "common_values End" << std::endl;
                if (prev != nullptr)
                {
                    //std::cout << "pev not null" << std::endl;
                    for (UniqueMatchDataPtr pathId : potential_answer) {
                        bool to_include = true;
                        // Knot limit checks
                        for (const Node* node : {this, prev}) {
                            try {
                                //std::cout << "try" << std::endl;
                                const std::optional<Container<std::pair<Limits, size_t>>::iterator> knot_path_limit = node->neighbours.at(this->current_symbol).paths.at(pathId);
                                if (knot_path_limit.has_value()) {
                                    //std::cout << "knot_path_limit has value" << std::endl;
                                    to_include &= changed_limits.at_or_create(knot_path_limit.value()).first.min == 0;
                                    //std::cout << "to_include" << to_include << std::endl;
                                }
                            }
                            catch (std::out_of_range& e) {
                                //std::cout << "catch called" << std::endl;
                            }
                        }
                        if (to_include) {
                            answer.push_back(pathId);
                        }
                    }
                    return answer;
                }
                else 
                    return potential_answer;
            }
            return {};
        }
        //std::cout << "Haha" << std::endl;
        std::vector<UniqueMatchDataPtr> answer;
        const symbol<char_t> current = symbol<char_t>(*begin);
        for (symbol<char_t> to_test : {current, symbol<char_t>::Any, symbol<char_t>::None}) {
            if (auto it = this->neighbours.find(to_test); it != this->neighbours.end()) {
                size_t ind = 0;
                std::vector<UniqueMatchDataPtr> new_paths;
                new_paths.reserve(paths.size());
                std::map<UniqueMatchDataPtr, std::pair<Limits, size_t>> current_paths;
                for (const auto [pathId, limits_ptr] : it->second.paths) {
                    if (limits_ptr.has_value() && !changed_limits.at_or_create(limits_ptr.value()).first.is_allowed_to_repeat()) continue;
                    if (prev != nullptr) {
                        if (paths[ind] > pathId) continue;
                        while (ind < paths.size() && paths[ind] < pathId) ind ++;
                        if (ind == paths.size()) break;
                    }
                    if (prev == nullptr || paths[ind] == pathId) {
                        new_paths.push_back(pathId);
                        if (!limits_ptr.has_value()) continue;
                        auto& current = changed_limits.at_or_create(limits_ptr.value());
                        current_paths.emplace(pathId, current);
                        --current.first;
                    }
                }
                if (!new_paths.empty()) {
                    if (to_test != symbol<char_t>::None) begin ++;
                    for (auto match : it->second.to->match_helper(begin, end, new_paths, this, changed_limits)) {
                        answer.push_back(match);
                    }
                    if (to_test != symbol<char_t>::None) begin --;
                    for (const auto [pathId, old_limits] : current_paths) {
                        changed_limits[it->second.paths.at(pathId).value()] = old_limits;
                    }
                }
            }
        }
        return std::move(answer);
    }

    template<typename UniqueMatchDataPtr, typename char_t>
    template<typename T>
    std::vector<UniqueMatchDataPtr> Node<UniqueMatchDataPtr, char_t>::common_values(const std::vector<UniqueMatchDataPtr>& sorted, const std::map<UniqueMatchDataPtr, T>& paths) {
        std::vector<UniqueMatchDataPtr> answer;
        if (sorted.empty()) {
            for (const auto [k, _] : paths)
                answer.push_back(k);
            return answer;
        }
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
}