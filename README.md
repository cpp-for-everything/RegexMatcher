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
