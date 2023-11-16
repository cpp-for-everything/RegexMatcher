#ifndef SYMBOL_H
#define SYMBOL_H

#include <string>

namespace URLMatcher {
    /**
     * @brief Contains the characters and additional attributes for wildcard symbols and Null symbols
     * 
     */
    template<typename char_t>
    struct symbol {
        char_t ch;
        bool wildcard;
        bool none;

        static const symbol Any;
        static const symbol None;
        static const symbol EOR; // end-of-regex

        symbol() : symbol(symbol<char_t>::None) {}
        symbol(char_t s) : ch(s), wildcard(false), none(false) {}
        symbol(char_t s, bool w, bool n) : ch(s), wildcard(w), none(n) {}

        inline bool operator== (const symbol& s) const {
            return (wildcard == s.wildcard) && (none == s.none) && (ch == s.ch);
        }
        inline bool operator!= (const symbol<char_t>& s) const {
            return (wildcard != s.wildcard) || (none != s.none) || (ch != s.ch);
        }

        bool operator<(const symbol<char_t>& s) const {
            if (ch == s.ch) {
                if (wildcard == s.wildcard)
                    return none < s.none;
                return wildcard < s.wildcard;
            }
            return (ch < s.ch);
        }

        inline std::basic_string<char_t> to_string() const {
            if (*this == symbol::Any) return "wildcard";
            if (*this == symbol::None) return "(empty)";
            if (*this == symbol::EOR) return "EOR";
            return std::basic_string<char_t>(1, ch);
        }
    };

}

#include "../src/symbol.cpp"

#endif
