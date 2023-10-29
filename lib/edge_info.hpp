#include <optional>
#include "limits.hpp"

/**
 * @brief Class containing the list of regexes using the given edge
 * 
 * @tparam T type of the reference to the match data
 */
template<typename T, typename Node>
struct EdgeInfo {
    std::map<T, std::optional<Limits>> paths; // each path may have different requirements for how many times should the edge be repeated.
    Node* to;
};