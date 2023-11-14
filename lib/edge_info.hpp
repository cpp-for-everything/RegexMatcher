#include "limits.hpp"

#ifndef EDGE_INFO_H
#define EDGE_INFO_H

#include <map>
#include <list>
#include <optional>

/**
 * @brief Class containing the list of regexes using the given edge
 * 
 * @tparam T type of the reference to the match data
 */
template<typename T, typename Node>
struct EdgeInfo {
    std::map<T, std::optional<std::list<Limits>::iterator>> paths; // each path may have different requirements for how many times should the edge be repeated.
    Node* to;
};

#endif