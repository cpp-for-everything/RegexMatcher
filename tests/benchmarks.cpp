/**
 * @file benchmarks.cpp
 * @brief Google Benchmark tests comparing RegexMatcher vs std::regex
 *
 * Run with: ./build/tests/benchmarks.exe (PowerShell/CMD on Windows)
 * Note: Git Bash may cause segfaults due to MSYS2 compatibility issues
 */

#include <benchmark/benchmark.h>
#include <matcher/core.hpp>
#include <regex>
#include <string>
#include <vector>

// ============================================================================
// Test Data
// ============================================================================

static const std::vector<std::string> SIMPLE_PATTERNS = {
    "a(bc)d", "(a)(b)(c)", "a(b|c)d", "a(bc)+d", "a(bc)*d"
};

static const std::vector<std::string> COMPLEX_PATTERNS = {
    "d(abc|def)*g+", "d(abc)*g+", "f[a-c]?d(ab|cd)*g+", "(d|e)f", "a{1,3}a"
};

static const std::vector<std::string> TEST_STRINGS = {
    "abcd", "abc", "abd", "abcbcd", "abcbcbcd",
    "dabcabcg", "ddefdefg", "fdabcdgg", "ef", "aaaa"
};

// ============================================================================
// Helper: Build matchers (called once per benchmark, not per iteration)
// ============================================================================

static matcher::RegexMatcher<int, char> build_simple_matcher() {
    matcher::RegexMatcher<int, char> m;
    for (size_t i = 0; i < SIMPLE_PATTERNS.size(); ++i) {
        m.add_regex(SIMPLE_PATTERNS[i], static_cast<int>(i));
    }
    return m;
}

static matcher::RegexMatcher<int, char> build_complex_matcher() {
    matcher::RegexMatcher<int, char> m;
    for (size_t i = 0; i < COMPLEX_PATTERNS.size(); ++i) {
        m.add_regex(COMPLEX_PATTERNS[i], static_cast<int>(i));
    }
    return m;
}

static std::vector<std::regex> build_simple_std_regexes() {
    std::vector<std::regex> v;
    for (const auto& p : SIMPLE_PATTERNS) v.emplace_back(p);
    return v;
}

static std::vector<std::regex> build_complex_std_regexes() {
    std::vector<std::regex> v;
    for (const auto& p : COMPLEX_PATTERNS) v.emplace_back(p);
    return v;
}

// ============================================================================
// Simple Pattern Benchmarks
// ============================================================================

static void BM_RegexMatcher_Simple_NoGroups(benchmark::State& state) {
    auto matcher = build_simple_matcher();
    const std::string test_str = "abcd";
    for (auto _ : state) {
        auto result = matcher.match(test_str);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_RegexMatcher_Simple_NoGroups);

static void BM_RegexMatcher_Simple_WithGroups(benchmark::State& state) {
    auto matcher = build_simple_matcher();
    const std::string test_str = "abcd";
    for (auto _ : state) {
        auto result = matcher.match_with_groups(test_str);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_RegexMatcher_Simple_WithGroups);

static void BM_StdRegex_Simple_NoGroups(benchmark::State& state) {
    auto regexes = build_simple_std_regexes();
    const std::string test_str = "abcd";
    for (auto _ : state) {
        std::vector<int> matches;
        for (size_t i = 0; i < regexes.size(); ++i) {
            if (std::regex_match(test_str, regexes[i])) {
                matches.push_back(static_cast<int>(i));
            }
        }
        benchmark::DoNotOptimize(matches);
    }
}
BENCHMARK(BM_StdRegex_Simple_NoGroups);

static void BM_StdRegex_Simple_WithGroups(benchmark::State& state) {
    auto regexes = build_simple_std_regexes();
    const std::string test_str = "abcd";
    for (auto _ : state) {
        std::vector<std::pair<int, std::smatch>> matches;
        for (size_t i = 0; i < regexes.size(); ++i) {
            std::smatch match;
            if (std::regex_match(test_str, match, regexes[i])) {
                matches.emplace_back(static_cast<int>(i), match);
            }
        }
        benchmark::DoNotOptimize(matches);
    }
}
BENCHMARK(BM_StdRegex_Simple_WithGroups);

// ============================================================================
// Complex Pattern Benchmarks
// ============================================================================

static void BM_RegexMatcher_Complex_NoGroups(benchmark::State& state) {
    auto matcher = build_complex_matcher();
    const std::string test_str = "dabcabcg";
    for (auto _ : state) {
        auto result = matcher.match(test_str);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_RegexMatcher_Complex_NoGroups);

static void BM_RegexMatcher_Complex_WithGroups(benchmark::State& state) {
    auto matcher = build_complex_matcher();
    const std::string test_str = "dabcabcg";
    for (auto _ : state) {
        auto result = matcher.match_with_groups(test_str);
        benchmark::DoNotOptimize(result);
    }
}
BENCHMARK(BM_RegexMatcher_Complex_WithGroups);

static void BM_StdRegex_Complex_NoGroups(benchmark::State& state) {
    auto regexes = build_complex_std_regexes();
    const std::string test_str = "dabcabcg";
    for (auto _ : state) {
        std::vector<int> matches;
        for (size_t i = 0; i < regexes.size(); ++i) {
            if (std::regex_match(test_str, regexes[i])) {
                matches.push_back(static_cast<int>(i));
            }
        }
        benchmark::DoNotOptimize(matches);
    }
}
BENCHMARK(BM_StdRegex_Complex_NoGroups);

static void BM_StdRegex_Complex_WithGroups(benchmark::State& state) {
    auto regexes = build_complex_std_regexes();
    const std::string test_str = "dabcabcg";
    for (auto _ : state) {
        std::vector<std::pair<int, std::smatch>> matches;
        for (size_t i = 0; i < regexes.size(); ++i) {
            std::smatch match;
            if (std::regex_match(test_str, match, regexes[i])) {
                matches.emplace_back(static_cast<int>(i), match);
            }
        }
        benchmark::DoNotOptimize(matches);
    }
}
BENCHMARK(BM_StdRegex_Complex_WithGroups);

// ============================================================================
// Batch Benchmarks (Multiple test strings)
// ============================================================================

static void BM_RegexMatcher_Batch_NoGroups(benchmark::State& state) {
    auto matcher = build_complex_matcher();
    for (auto _ : state) {
        for (const auto& test_str : TEST_STRINGS) {
            auto result = matcher.match(test_str);
            benchmark::DoNotOptimize(result);
        }
    }
}
BENCHMARK(BM_RegexMatcher_Batch_NoGroups);

static void BM_RegexMatcher_Batch_WithGroups(benchmark::State& state) {
    auto matcher = build_complex_matcher();
    for (auto _ : state) {
        for (const auto& test_str : TEST_STRINGS) {
            auto result = matcher.match_with_groups(test_str);
            benchmark::DoNotOptimize(result);
        }
    }
}
BENCHMARK(BM_RegexMatcher_Batch_WithGroups);

static void BM_StdRegex_Batch_NoGroups(benchmark::State& state) {
    auto regexes = build_complex_std_regexes();
    for (auto _ : state) {
        for (const auto& test_str : TEST_STRINGS) {
            std::vector<int> matches;
            for (size_t i = 0; i < regexes.size(); ++i) {
                if (std::regex_match(test_str, regexes[i])) {
                    matches.push_back(static_cast<int>(i));
                }
            }
            benchmark::DoNotOptimize(matches);
        }
    }
}
BENCHMARK(BM_StdRegex_Batch_NoGroups);

static void BM_StdRegex_Batch_WithGroups(benchmark::State& state) {
    auto regexes = build_complex_std_regexes();
    for (auto _ : state) {
        for (const auto& test_str : TEST_STRINGS) {
            std::vector<std::pair<int, std::smatch>> matches;
            for (size_t i = 0; i < regexes.size(); ++i) {
                std::smatch match;
                if (std::regex_match(test_str, match, regexes[i])) {
                    matches.emplace_back(static_cast<int>(i), match);
                }
            }
            benchmark::DoNotOptimize(matches);
        }
    }
}
BENCHMARK(BM_StdRegex_Batch_WithGroups);

// ============================================================================
// Scaling Benchmarks (varying number of patterns)
// ============================================================================

static void BM_RegexMatcher_ScalePatterns(benchmark::State& state) {
    const int num_patterns = state.range(0);
    matcher::RegexMatcher<int, char> matcher;
    
    for (int i = 0; i < num_patterns; ++i) {
        std::string pattern = "pattern" + std::to_string(i) + "(x)+";
        matcher.add_regex(pattern, i);
    }
    matcher.add_regex(std::string("test(abc)+end"), num_patterns);
    
    std::string test_str = "testabcabcend";
    
    for (auto _ : state) {
        auto result = matcher.match(test_str);
        benchmark::DoNotOptimize(result);
    }
    state.SetComplexityN(num_patterns);
}
BENCHMARK(BM_RegexMatcher_ScalePatterns)->RangeMultiplier(2)->Range(1, 64)->Complexity();

static void BM_StdRegex_ScalePatterns(benchmark::State& state) {
    const int num_patterns = state.range(0);
    std::vector<std::regex> regexes;
    
    for (int i = 0; i < num_patterns; ++i) {
        std::string pattern = "pattern" + std::to_string(i) + "(x)+";
        regexes.emplace_back(pattern);
    }
    regexes.emplace_back("test(abc)+end");
    
    std::string test_str = "testabcabcend";
    
    for (auto _ : state) {
        std::vector<int> matches;
        for (size_t i = 0; i < regexes.size(); ++i) {
            if (std::regex_match(test_str, regexes[i])) {
                matches.push_back(static_cast<int>(i));
            }
        }
        benchmark::DoNotOptimize(matches);
    }
    state.SetComplexityN(num_patterns);
}
BENCHMARK(BM_StdRegex_ScalePatterns)->RangeMultiplier(2)->Range(1, 64)->Complexity();

// ============================================================================
// Route and word workloads: which of N patterns match each input in full.
// Each benchmark iteration looks up every input once; the counter "inputs" gives the
// rate per input. The match_* variants compare the entry points of RegexMatcher; the
// Simulation variant turns the lookup table off; StdRegex tries one std::regex per pattern.
// ============================================================================

struct Workload {
    std::vector<std::string> patterns;
    std::vector<std::string> inputs;
};

// 3 patterns per resource: /api/v1/resI/[0-9]+, .../items, .../items/[a-z]+
static Workload routes_workload(int resources) {
    Workload w;
    for (int i = 0; i < resources; ++i) {
        const std::string r = "/api/v1/res" + std::to_string(i);
        w.patterns.push_back(r + "/[0-9]+");
        w.patterns.push_back(r + "/[0-9]+/items");
        w.patterns.push_back(r + "/[0-9]+/items/[a-z]+");
    }
    for (int i : {0, resources / 2, resources - 1}) {
        const std::string r = "/api/v1/res" + std::to_string(i);
        w.inputs.push_back(r + "/12345");
        w.inputs.push_back(r + "/12345/items");
        w.inputs.push_back(r + "/12345/items/widget");
    }
    w.inputs.push_back("/api/v1/nothing/12345");
    w.inputs.push_back("/static/app.js");
    return w;
}

static Workload words_workload(int count) {
    Workload w;
    for (int i = 0; i < count; ++i) w.patterns.push_back("prefix_word" + std::to_string(i));
    for (int i : {0, count / 2, count - 1}) w.inputs.push_back("prefix_word" + std::to_string(i));
    w.inputs.push_back("prefix_wordX");
    w.inputs.push_back("other");
    return w;
}

static Workload workload_for(const benchmark::State& state) {
    const int n = static_cast<int>(state.range(1));
    return state.range(0) == 0 ? routes_workload(n) : words_workload(n);
}

static matcher::RegexMatcher<int, char> build_workload_matcher(const Workload& w,
                                                               const matcher::TableLimits& limits = {}) {
    matcher::RegexMatcher<int, char> m;
    m.set_table_limits(limits);
    for (size_t i = 0; i < w.patterns.size(); ++i) m.add_regex(w.patterns[i], static_cast<int>(i));
    m.compile();
    return m;
}

static void label_workload(benchmark::State& state, const Workload& w) {
    state.SetLabel((state.range(0) == 0 ? "routes, " : "words, ") + std::to_string(w.patterns.size()) + " patterns");
    state.counters["inputs"] = benchmark::Counter(static_cast<double>(state.iterations() * w.inputs.size()),
                                                  benchmark::Counter::kIsRate);
}

static void BM_Workload_Match(benchmark::State& state) {
    const Workload w = workload_for(state);
    const auto m = build_workload_matcher(w);
    for (auto _ : state) {
        for (const auto& s : w.inputs) {
            auto result = m.match(s);
            benchmark::DoNotOptimize(result);
        }
    }
    label_workload(state, w);
}

static void BM_Workload_ForEachMatch(benchmark::State& state) {
    const Workload w = workload_for(state);
    const auto m = build_workload_matcher(w);
    for (auto _ : state) {
        size_t count = 0;
        for (const auto& s : w.inputs) m.for_each_match(s, [&count](int) { ++count; });
        benchmark::DoNotOptimize(count);
    }
    label_workload(state, w);
}

static void BM_Workload_MatchInto(benchmark::State& state) {
    const Workload w = workload_for(state);
    const auto m = build_workload_matcher(w);
    std::vector<int> ids;
    for (auto _ : state) {
        for (const auto& s : w.inputs) {
            m.match_into(s, ids);
            benchmark::DoNotOptimize(ids.data());
        }
    }
    label_workload(state, w);
}

static void BM_Workload_MatchesAny(benchmark::State& state) {
    const Workload w = workload_for(state);
    const auto m = build_workload_matcher(w);
    for (auto _ : state) {
        size_t any = 0;
        for (const auto& s : w.inputs) any += m.matches_any(s) ? 1 : 0;
        benchmark::DoNotOptimize(any);
    }
    label_workload(state, w);
}

static void BM_Workload_Simulation(benchmark::State& state) {
    const Workload w = workload_for(state);
    matcher::TableLimits off;
    off.max_states = 0;
    const auto m = build_workload_matcher(w, off);
    for (auto _ : state) {
        size_t count = 0;
        for (const auto& s : w.inputs) m.for_each_match(s, [&count](int) { ++count; });
        benchmark::DoNotOptimize(count);
    }
    label_workload(state, w);
}

static void BM_Workload_StdRegex(benchmark::State& state) {
    const Workload w = workload_for(state);
    std::vector<std::regex> regexes;
    for (const auto& p : w.patterns) regexes.emplace_back(p);
    for (auto _ : state) {
        size_t count = 0;
        for (const auto& s : w.inputs) {
            for (const auto& r : regexes) count += std::regex_match(s, r) ? 1 : 0;
        }
        benchmark::DoNotOptimize(count);
    }
    label_workload(state, w);
}

// Arguments: {0 = routes (resources), 1 = words (count)}
#define WORKLOAD_ARGS ->Args({0, 3})->Args({0, 21})->Args({0, 85})->Args({0, 341})->Args({1, 16})->Args({1, 256})->Args({1, 4096})
BENCHMARK(BM_Workload_Match) WORKLOAD_ARGS;
BENCHMARK(BM_Workload_ForEachMatch) WORKLOAD_ARGS;
BENCHMARK(BM_Workload_MatchInto) WORKLOAD_ARGS;
BENCHMARK(BM_Workload_MatchesAny) WORKLOAD_ARGS;
BENCHMARK(BM_Workload_Simulation) WORKLOAD_ARGS;
BENCHMARK(BM_Workload_StdRegex) WORKLOAD_ARGS;
#undef WORKLOAD_ARGS

BENCHMARK_MAIN();
