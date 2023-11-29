#ifndef LIMITS_H
#define LIMITS_H

#include <optional>
#include <list>
#include <vector>
#include <sstream>

namespace URLMatcher {

    template<typename T>
    using Container = std::list<T>;

    struct Limits {
        size_t min;
        std::optional<size_t> max;
        
        Limits() : Limits (0, std::nullopt) {}
        
        Limits(size_t min, std::nullopt_t) {
            this->min = min;
            this->max = std::nullopt;
        }
        
        Limits(std::nullopt_t, size_t max) : Limits (0, max) {}
        
        Limits(size_t min, size_t max) {
            this->min = min;
            this->max = max;
        }

        static const Limits common_edge;
        static const Limits zero_or_once;
        static const Limits once_or_more;
        static const Limits zero_or_more;

        static std::string to_string(const std::optional<Container<std::pair<Limits, size_t>>::iterator>& it) {
            if (!it.has_value()) { return ""; }
            if (!it.value()->first.max.has_value()) { return (std::stringstream() << "(" << it.value()->first.min << "+)").str(); }
            return (std::stringstream() << "(" << it.value()->first.min << "..." << it.value()->first.max.value() << ")").str();
        }

        static std::string to_string(const Limits& a) {
            if (!a.max.has_value()) { return (std::stringstream() << "(" << a.min << "+)").str(); }
            return (std::stringstream() << "(" << a.min << "..." << a.max.value() << ")").str();
        }

        Limits& operator--() {
            if (min > 0) min --;
            if (max.has_value() && max.value() > 0) max = max.value() - 1;
            return *this;
        }

        bool is_required() const {
            return min > 0;
        }

        bool is_allowed_to_repeat() const {
            return !max.has_value() || (max.value() > 0 && this->min <= this->max.value());
        }
    };
}

#include "../src/limits.cpp"

#endif