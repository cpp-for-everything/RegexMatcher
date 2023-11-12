#include "../lib/symbol.hpp"

template<typename char_t>
const symbol<char_t> symbol<char_t>::Any{'\0', true, false};

template<typename char_t>
const symbol<char_t> symbol<char_t>::None{'-', false, true};

template<typename char_t>
const symbol<char_t> symbol<char_t>::EOR{'\0', false, true};