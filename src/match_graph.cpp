#pragma once

#include <match_graph.hpp>

namespace Regex {
    template<typename MatchData, typename StringT>
    Matcher<MatchData, StringT>& Matcher<MatchData, StringT>::add(StringT regex, const MatchData&& data) {
        return this;
    }
}