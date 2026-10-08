# Changelog

## 2.1.0.0 (2026-10-08)

This release adds the HTTP route matcher. The regex engine is the one of 2.0.0.1, unchanged.

### Added
- The HTTP route matcher, `include/matcher/route.hpp` and `include/matcher/route/` (target
  `RegexMatcher::route`, C++23, header-only): route patterns with literal, typed (`u64`,
  `i64`), plain and catch-all segments; a flat routing table built at run time or while
  compiling; a lookup that allocates nothing and returns the captured values as views into the
  path; the checked front end (patterns as template arguments, handler checks, compile-time
  tables of route declarations with handlers). Path semantics in `docs/route-semantics.md`.
- `RuntimeTable` and `make_runtime_table`: a table built at run time in one block aligned to a
  page, its arrays where a compile-time table of the same routes has them (`table_layout`); a
  compile-time table declared `alignas(kPageAlign)` then has every element at the same offset
  into a page.
- Tests of the route matcher (`tests/route/`), with compile-time tables of up to 1,000 routes
  and negative-compilation tests.
- CMake options `REGEXMATCHER_BUILD_TESTS`, `REGEXMATCHER_BUILD_BENCHMARKS`,
  `REGEXMATCHER_SANITIZER`, `REGEXMATCHER_MSAN_LIBCXX`, `REGEXMATCHER_CT_STEPS`,
  `REGEXMATCHER_CT_JOBS`.

### Changed
- CMake 3.25 or newer. Each target states its language standard: `RegexMatcher::core` C++17,
  `RegexMatcher::route` C++23.
- The exported targets carry the namespace `RegexMatcher::` (`RegexMatcher::core`,
  `RegexMatcher::route`), and the package's include directory is `include`.
- Tests and benchmarks are built only when asked for (tests by default at the top level), and
  GoogleTest and Google Benchmark are fetched only then. The test executable is no longer
  installed.

### Breaking for projects that use the installed package
- The exported engine target was `core` and is now `RegexMatcher::core`.
- `RegexMatcher_INCLUDE_DIRS` is now `<prefix>/include`.
- The minimum CMake version rose from 3.16.3 to 3.25.

A project that adds this repository with `add_subdirectory` still has the target
`RegexMatcher`, and now also `RegexMatcher::core` and `RegexMatcher::route`.

### Known issue
- The regex engine checks a repeat's lower bound, where more of the pattern follows the
  repeat, only in a matcher on which `compile()` has run. Without `compile()`,
  `a[0-9]{2,3}c` matches `a1c` and `x[0-9]{3}z` matches `x1z`. Call `compile()` after the
  last `add_regex` and before matching. 2.0.0.1 behaves the same way.

## 2.0.0.1 (commit d16f30a8)

The regex engine as it has been on `main` since 2026-09-19. This file starts with this
version; earlier changes are in the commit history.

### Fixed
- Matching no longer writes to the shared graph. Each match keeps its own repeat counters, so
  several threads can match on one `RegexMatcher` at once.
- In a matcher on which `compile()` has run, a repeat's lower bound holds where more of the
  pattern follows the repeat (`a[0-9]{2,3}c` no longer matches `a1c`), and `{n}` means
  exactly n repetitions.

### Added
- `compile()`, called after the last `add_regex` and before matching. It records, per edge,
  which regexes stay alive, joins runs of literal characters into one comparison, and records
  the repeat bounds that leaving a state must check. Matching without it takes the general
  path.

### Changed
- A character class is one state that holds its members, instead of one state per member.
- `match` and `match_with_groups` return their results sorted by regex.
- `MatchResult::groups` is a sorted run with keyed lookup instead of a `std::map`;
  `groups.at(0)` still means group zero.
- States are allocated in blocks, and edges and per-match state are kept in flat runs, so a
  match allocates less.
