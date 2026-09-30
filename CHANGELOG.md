# Changelog

## Unreleased (the next major version; its number is not decided yet)

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
- Chain nodes: a node with no route and one literal child only, with the nodes below it that
  are the same, is one node of kind `kNodeChain` (its segments one run in the arena,
  `Node::chain` bytes long); the lookup answers as before. `MATCHER_ROUTE_ON_BACKTRACK()`, empty
  by default, is called at every pop of the walk's backtracking stack.
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
