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

    symbol(char s) : ch(s), wildcard(false), none(false) {}
    symbol(char s, bool w, bool n) : ch(s), wildcard(w), none(n) {}
};

const symbol symbol::Any{'\0', true, false};
const symbol symbol::None{'\0', false, true};