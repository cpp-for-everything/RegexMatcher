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

BENCHMARK_MAIN();
