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
#include "regex_matcher.h"

RegexMatcher matcher;
matcher.add_pattern("^a(bc)*d$");
matcher.add_pattern("^123[0-9]+abc$");

if (matcher.match("123456abc")) {
    std::cout << "Match found!" << std::endl;
}
```

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

Compilers checked so far: clang 18.1.3 and 22.1.8, gcc 14.2 and 16.2.1 (Linux), MSVC 19.51 and
clang-cl 22.1.0 (Windows). With clang 22.1.8, gcc 16.2.1, MSVC 19.51 and clang-cl 22.1.0 the
whole test suite passes, the compile-time tables of 1,000 routes included. Tables of about 1,000 routes
built while compiling need a larger constant-evaluation budget than compilers allow by default
(`-fconstexpr-steps`, `/constexpr:steps`); the tests use `REGEXMATCHER_CT_STEPS`.

## Benchmarks and Performance

RegexMatcher achieves competitive throughput compared to existing open-source libraries, especially in workloads with many patterns and strict matching requirements. Benchmarking against traditional engines (e.g., PCRE, std::regex) demonstrates its superior performance in multi-pattern deterministic scenarios.

> More detailed evaluation and formal analysis can be found in the [scientific paper](https://doi.org/10.1109/TELECOM63374.2024.10812214).

## Applications

RegexMatcher is designed for real-world scenarios where correctness and speed are critical. This includes:

- Parsing and validating URLs and network traffic
- Lexical analysis for domain-specific languages
- High-performance rule engines for web filtering
- Static or runtime analysis of log patterns and structured data

## License

<!-- SPDX-License-Identifier: GPL-3.0-only -->

This project is dual-licensed:

- **Open Source**: [GNU General Public License v3.0](LICENSE) (GPLv3)
- **Commercial**: available for proprietary use; see [LICENSING.md](LICENSING.md) for details

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
