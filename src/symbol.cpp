#include "../lib/symbol.hpp"

const symbol symbol::Any{'\0', true, false};
const symbol symbol::None{'-', false, true};
const symbol symbol::EOR{'\0', false, true};