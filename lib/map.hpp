#ifndef EXTENDED_MAP_H
#define EXTENDED_MAP_H

#include "limits.hpp"
#include <unordered_map>

template<>
struct std::hash<typename URLMatcher::Container<std::pair<URLMatcher::Limits, size_t>>::const_iterator>
{
    std::size_t operator()(typename URLMatcher::Container<std::pair<URLMatcher::Limits, size_t>>::const_iterator const& iter) const noexcept
    {
        return iter->second;
    }
};

namespace URLMatcher {

    template<typename T>
    class Map : public std::unordered_map<typename Container<T>::const_iterator, T> {
    public:
        inline T& at_or_create(const typename Container<T>::const_iterator& k) {
            return this->emplace(k, *k).first->second;
        }
    };

}

#endif