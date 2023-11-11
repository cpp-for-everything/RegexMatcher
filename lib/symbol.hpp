#ifndef SYMBOL_H
#define SYMBOL_H

#include <string>

/**
 * @brief Contains the characters and additional attributes for wildcard symbols and Null symbols
 * 
 */
struct symbol {
    char ch;
    bool wildcard;
    bool none;

    static const symbol Any;
    static const symbol None;
    static const symbol EOR; // end-of-regex

    symbol() : symbol(symbol::None) {}
    symbol(char s) : ch(s), wildcard(false), none(false) {}
    symbol(char s, bool w, bool n) : ch(s), wildcard(w), none(n) {}

    inline bool operator== (const symbol& s) const {
        return (wildcard == s.wildcard) && (none == s.none) && (ch == s.ch);
    }
    inline bool operator!= (const symbol& s) const {
        return (wildcard != s.wildcard) || (none != s.none) || (ch != s.ch);
    }

    bool operator<(const symbol& s) const {
        if (ch == s.ch) {
            if (wildcard == s.wildcard)
                return none < s.none;
            return wildcard < s.wildcard;
        }
        return (ch < s.ch);
    }

    inline std::string to_string() const {
        if (*this == symbol::Any) return "wildcard";
        if (*this == symbol::None) return "(empty)";
        if (*this == symbol::EOR) return "EOR";
        return std::string(1, ch);
    }
};

#endif
