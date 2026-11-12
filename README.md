# RegexMatcher: High-Performance Multi-Pattern Regex Engine in C++

### Matching Text from Start to Finish Against Multiple Regular Expressions

---

## Overview

**RegexMatcher** is a high-performance C++ library for matching input text against multiple regular expressions simultaneously, from the beginning to the end of a string. It is designed for efficiency and accuracy, enabling deterministic behavior even in large-scale, high-throughput systems such as web servers, content filters, or custom compilers.

This project implements an advanced data structure inspired by finite automata and combines it with regex parsing to allow effective matching, leveraging an iterative algorithmic design. Unlike traditional regex engines that operate one pattern at a time, RegexMatcher is optimized for batch processing of many regular expressions with bounded overhead.

## Scientific Foundation

The core algorithmic approach of this project is described in the following peer-reviewed publication:

> I. Stankov and A. Tsvetanov, "Matching Text from Start to Finish Against Multiple Regular Expressions,"
> 2024 32nd National Conference with International Participation (TELECOM), Sofia, Bulgaria, pp. 1–4, doi: 10.1109/TELECOM63374.2024.10812214.

The paper introduces a hybrid model that blends deterministic finite automata (DFA) with trie-based backtracking and partial Aho-Corasick integration. The algorithm is designed to guarantee full-string matches and minimize ambiguity, supporting practical applications in:

- Web server request parsing
- URL pattern recognition
- Telecom stream filtering
- High-throughput automated testing tools

## Keywords
>
> **Uniform resource locators, C++ languages, Data structures, Web servers, Iterative algorithms, Telecommunications, Testing, Regex, Pattern matching, Aho-Corasick**

## Features

- ⚡ **Multi-pattern support**: Match multiple regex patterns simultaneously from start to end.
- 🧠 **Deterministic traversal**: Uses a hybrid state machine model with no backtracking on successful paths.
- 🛠 **Written in modern C++**: Clean, modular, and efficient C++17 implementation.
- 🔍 **Debuggable and testable**: Includes testing harnesses and debugging modes for state machine introspection.
- 🌐 **Designed for scalability**: Suitable for integration into web servers, telecom processing units, and parsers.

## Quick Start

Clone and Build

```bash
git clone https://github.com/cpp-for-everything/RegexMatcher.git
cd RegexMatcher
mkdir build && cd build
cmake ..
make
```

## Example Usage

You can construct a matcher and feed it multiple regexes like so:

```cpp
#include <matcher/core.hpp>

#include <iostream>
#include <string>

matcher::RegexMatcher<int, char> matcher;  // the int is the id given to each pattern
matcher.add_regex(std::string("a(bc)*d"), 0);
matcher.add_regex(std::string("123[0-9]+abc"), 1);

for (int id : matcher.match(std::string("123456abc"))) {
    std::cout << "Pattern " << id << " matches\n";
}
```

A pattern always has to match the whole text, so it takes no `^` or `$`.

Supported syntax: literal characters, `.` (any character but a line terminator, as in
ECMAScript), sets `[abc]` and ranges `[a-z]`, groups `( )`, alternation `|`, the repeats `*`, `+`,
`?`, `{n}`, `{n,}` and `{n,m}`, a `?` after a repeat (lazy, as in ECMAScript), and `\` before a
punctuation character. A malformed pattern, or one that uses anything else (`^`, `$`, `[^...]`,
`[]`, `\d`, `\w`, `\s` and other escapes of letters or digits, a repeat right after a repeat such
as `a**`), makes `add_regex` throw `std::invalid_argument` and leaves the matcher unchanged.

How it works: each pattern becomes an automaton with empty moves. A count such as `{2,4}` is
written out as copies of the repeated part, so the automaton has no counters. A pattern whose counts
write out more than 65536 states is rejected with `std::invalid_argument`, for example
`(a{1000}){1000}`. Patterns share the states of identical leading items: `/users/[0-9]+` and
`/users/[0-9]+/posts` share all of `/users/[0-9]+`. Nothing else is shared, so sharing never changes
what a pattern matches. A pattern with groups and several top level alternatives shares nothing.

Lookup table: for `char` (any 8 bit `char_t`), `match` and the other lookups below do not walk the
automaton. On the first lookup after a change the matcher builds a deterministic table from it by
subset construction: the 256 byte values are grouped into classes that every pattern treats alike,
and each table state lists the ids it accepts. A lookup is then one table load per byte, and it
stops early when no pattern can match any more. `compile()` builds the table ahead, so the first
lookup is not slow. `add_regex` and `set_table_limits` drop the table; the next lookup builds a new
one.

Caps and fallback: `set_table_limits(TableLimits)` caps the table at `max_states` states (default
65536, the dead state included) and `max_bytes` bytes (default 32 MiB, the build included). The full
build also stops when its work passes a bound proportional to the size of the automaton, so a set
of patterns whose table explodes, such as `(a|b)*a(a|b){12}` with many others, stops early. The
matcher then fills a lazy table during lookups, one state at a time, under the same caps. A lookup
that would pass them simulates the automaton instead. `build_ahead = false` skips the full build;
`max_states = 0` turns the table off. Every mode gives the same answers. `table_stats()` tells which
mode a matcher uses, its size, why the full build stopped, and how many lookups were simulated.
A `char_t` wider than 8 bits has no table: its lookups simulate the automaton.

Simulation: it steps the set of live states one character at a time and never backtracks. Its time
is bounded by the length of the text times the size of the automaton. It reuses scratch buffers kept
by the matcher, so a call does not allocate once they are as large as the automaton needs.

Lookups: `match(text)` returns a `std::vector` of the matching ids; it allocates only when something
matches. Three forms allocate nothing when the full table is in use:

```cpp
size_t n = matcher.for_each_match(text, [](int id) { /* ... */ });  // return false to stop early
std::vector<int> ids;                       // reused from call to call
matcher.match_into(text, ids);              // replaces the contents of ids
bool any = matcher.matches_any(text);
```

Each gives exactly the ids of `match`, in the same order. The text is any range with `std::cbegin`
and `std::cend`, such as `std::string` or `std::string_view`, passed by reference; a C array is
refused at compile time, because its final `'\0'` would be matched.

Answers: `match` agrees with `std::regex_match` (ECMAScript) on the syntax above. The tests check
this on random patterns, alone and many in one matcher. `match` and `match_with_groups` return each
matching id once, in ascending order. The tests also check every table mode and every lookup form
against `std::regex_match`.

Captures: `match_with_groups` follows ECMAScript: the match a backtracking engine finds first, a
new pass of a repeat forgets the captures of the last pass, and an optional pass that consumes
nothing does not count. The two `std::regex` libraries the tests ran against differ from this, and
from each other, in cases the tests found where a repeated group can match the empty string: libstdc++
keeps captures of earlier passes and counts empty passes; the Microsoft library can stop a greedy
loop after an empty pass. Group ids start at 0.

Captures stay on the simulation. When the full table shows that no pattern matches,
`match_with_groups` returns at once.

Threads: every lookup (`match`, `for_each_match`, `match_into`, `matches_any`,
`match_with_groups`), `compile` and `table_stats` may run on one matcher from several threads at the
same time. `add_regex` and `set_table_limits` must not run while another thread uses the matcher.
The table is built once, under a mutex, by whichever lookup comes first; after that a lookup pays one
atomic load and one branch to find it. A lazy table is read and filled under its own mutex, so
lookups on it do not run in parallel. A copy of a matcher builds its own table; a move takes the
table along. Matching uses no recursion, so a long text does not overflow the native stack. Its
memory grows with the size of the automaton, not with the length of the text.

## Route Matcher

RegexMatcher also ships an HTTP route matcher, `include/matcher/route.hpp` (target
`RegexMatcher::route`, C++23, header-only, standard library only). It maps a method and a
request path to a route and the values its parameters capture, allocates nothing per lookup,
and can check routes and their handlers when the program is compiled.

```cpp
#include <matcher/route.hpp>
using namespace matcher::route;

// At run time: routes from anywhere, then one lookup per request.
const RouteSpec specs[] = {{index(Method::Get), "/users/{id:u64}", 0},
                           {index(Method::Get), "/users/me", 1},
                           {index(Method::Get), "/files/{*path}", 2}};
const Built table = build_table(specs);  // table.error names a bad or duplicate route
const Match m = find(table.view(), Method::Get, "/users/42");
// m.route == 0, m.values()[0] == "42", a view into the path

// While compiling: a malformed pattern, a handler that does not fit its pattern, or two
// routes that match the same paths stop the compilation.
std::string get_user(std::uint64_t id, Request& req);
using H = handlers<std::string, Request&>;
inline constexpr H::decl api[] = {H::get<"/users/{id:u64}", &get_user>()};
inline constexpr auto api_table = make_route_table<api>();
```

`make_runtime_table(specs)` builds the same table into one block aligned to a page
(`RuntimeTable`), with its arrays where a compile-time table of the same routes has them; a
compile-time table declared `alignas(kPageAlign)` then lays out every element alike.

Patterns: literal segments, `{name}`, typed `{name:u64}` and `{name:i64}`, and a last
`{*name}`. The most specific route wins whatever the registration order; a path that only
another method's routes match is "method not allowed". The rules are in
[docs/route-semantics.md](docs/route-semantics.md).

Compilers checked so far: clang 18.1.3 and gcc 14.2 (Linux), MSVC 19.51 and clang-cl 22.1.0
(Windows; the compile-time tables of 1,000 routes not yet built there). Tables of about 1,000 routes
built while compiling need a larger constant-evaluation budget than compilers allow by default
(`-fconstexpr-steps`, `/constexpr:steps`); the tests use `REGEXMATCHER_CT_STEPS`.

## Benchmarks and Performance

The task measured: for each input, find which of N patterns match it in full. It was measured
against two classes of engine: multi-pattern engines that compile all patterns into one matcher
(RE2::Set, Hyperscan) and engines that try one compiled regex per pattern (PCRE2 with JIT, RE2,
boost::regex, std::regex). The workloads were route sets, word lists, random patterns from the
differential test, inputs that match nothing, and inputs of 256 and 4096 bytes. In this benchmark,
on a development host, the callback form (`for_each_match`) took less time per input than
Hyperscan and RE2::Set on the inputs of up to 256 bytes. On the 4096 byte inputs Hyperscan took
less time, and RE2::Set took about the same. `match`, which returns a vector, pays for one
allocation whenever something matches. Numbers depend on the host, the compiler and the standard
library, so none are quoted here.

`tests/benchmarks.cpp` (Google Benchmark, `-DREGEXMATCHER_BUILD_BENCHMARKS=ON`) runs the route and
word workloads through every entry point, with the table off, and with `std::regex`, without
external engines.

> More detailed evaluation and formal analysis can be found in the [scientific paper](https://doi.org/10.1109/TELECOM63374.2024.10812214).

## Applications

RegexMatcher is designed for real-world scenarios where correctness and speed are critical. This includes:

- Parsing and validating URLs and network traffic
- Lexical analysis for domain-specific languages
- High-performance rule engines for web filtering
- Static or runtime analysis of log patterns and structured data

## License

<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

This project is dual-licensed:

- **Open Source**: [GNU General Public License v3.0](LICENSE) (GPLv3)
- **Commercial**: Available for proprietary use — see [LICENSING.md](LICENSING.md) for details

Copyright (C) 2025 Alex Tsvetanov

## Citation

If you use this library in academic work, please cite the following:

```bib
@inproceedings{stankov2024regexmatcher,
  author    = {I. Stankov and A. Tsvetanov},
  title     = {Matching Text from Start to Finish Against Multiple Regular Expressions},
  booktitle = {2024 32nd National Conference with International Participation (TELECOM)},
  year      = {2024},
  pages     = {1--4},
  doi       = {10.1109/TELECOM63374.2024.10812214}
}
```

## Contributing

Contributions are welcome! If you have suggestions, optimizations, or bug fixes, feel free to open a pull request or issue.
