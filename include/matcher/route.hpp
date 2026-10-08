#pragma once

// RegexMatcher's route matcher: route patterns, a flat routing table, and the lookup over it.
//
// Everything here is constexpr, so one table layout and one lookup function serve both a
// table built at run time (routes added one by one, then frozen) and a table built at
// compile time (a constexpr object from a fixed list of routes).
//
// Path semantics (docs/route-semantics.md):
//   - bytes are compared and captured as sent; nothing is percent-decoded;
//   - a parameter matches one non-empty segment, any bytes except '/';
//   - a catch-all matches a non-empty remainder of the path, taken as sent;
//   - a trailing slash is strict;
//   - at every segment a literal beats a typed parameter, which beats a plain parameter,
//     which beats a catch-all, whatever the order of registration;
//   - a path that no route of the request's method matches, but a route of another method
//     does, is "method not allowed" (405); otherwise "not found" (404).
//
// Layout: a method whose routes are all literal is answered by one exact-match hash table keyed
// by method and whole path. Every other method has a segment trie, one root per method,
// flattened into contiguous arrays; its literal routes live in the trie too. A RuntimeTable holds
// the four arrays in one page-aligned block, where a StaticTable of the same routes declared
// alignas(kPageAlign) holds them (table_layout).

#include <matcher/route/config.hpp>
#include <matcher/route/pattern.hpp>
#include <matcher/route/detail/words.hpp>
#include <matcher/route/table.hpp>
#include <matcher/route/lookup.hpp>
#include <matcher/route/static_table.hpp>
#include <matcher/route/runtime_table.hpp>
#include <matcher/route/method.hpp>
#include <matcher/route/checked.hpp>
