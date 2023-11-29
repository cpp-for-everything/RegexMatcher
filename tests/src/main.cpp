#include <matcher/core.hpp>
#include <iostream>
#include <regex>

#define enable(x,y) x.push_back(y);
#define disable(x,y)

#include <chrono>
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::milliseconds;
using namespace std::chrono_literals;

int main(int argc, char** argv) {
    {
        matcher::RegexMatcher<int, char> root;
        int num = 0;

        std::vector<std::string> regexes;
        enable(regexes, "d(abc|def)*g+")
        enable(regexes, "d(abc)*g+")
        enable(regexes, "a?")
        enable(regexes, "b|c")
        enable(regexes, "(d|e)f")
        enable(regexes, "f[a-c]?d(ab|cd)*g+")
        enable(regexes, "a{1,3}a")
        enable(regexes, "aaa")
        enable(regexes, "aa")

        duration<double, std::milli> total, alternative = 0ms;
        { 
            auto t1 = high_resolution_clock::now();
            for (std::string regex : regexes) {
                //std::cout << "'" << regex << "' is number " << ++num << std::endl;
                root.add_regex(regex, ++num);
            }
            auto t2 = high_resolution_clock::now();
            total = t2 - t1;
            std::cout << "Pre-computing " << total.count() << "ms (my)" << std::endl;
        }
        //root.print_list_of_edges();

        std::vector<std::string> texts;
        
        enable(texts, "")
        enable(texts, "a")
        enable(texts, "aa")
        enable(texts, "aaa")
        enable(texts, "aaaa")

        enable(texts, "")
        enable(texts, "a")
        enable(texts, "b")
        enable(texts, "c")
        enable(texts, "d")
        enable(texts, "df")
        enable(texts, "e")
        enable(texts, "ef")
        enable(texts, "fdg")
        enable(texts, "fdgg")
        enable(texts, "fd")

        enable(texts, "fdabgg")
        enable(texts, "fdababgg")
        enable(texts, "fdabcdgg")
        enable(texts, "fdcdcdgg")
        enable(texts, "fdacgg")

        enable(texts, "fadabgg")
        enable(texts, "fadababgg")
        enable(texts, "fadabcdgg")
        enable(texts, "fadcdcdgg")
        enable(texts, "fadacgg")

        enable(texts, "fbdabgg")
        enable(texts, "fbdababgg")
        enable(texts, "fbdabcdgg")
        enable(texts, "fbdcdcdgg")
        enable(texts, "fbdacgg")

        enable(texts, "fcdabgg")
        enable(texts, "fcdababgg")
        enable(texts, "fcdabcdgg")
        enable(texts, "fcdcdcdgg")
        enable(texts, "fcdacgg")

        enable(texts, "fccdabgg")
        enable(texts, "fccdababgg")
        enable(texts, "fccdabcdgg")
        enable(texts, "fccdcdcdgg")
        enable(texts, "fccdacgg")

        enable(texts, "dab")
        enable(texts, "dabcabc")
        enable(texts, "dabc")
        enable(texts, "ddefdef")
        enable(texts, "dabcg")
        enable(texts, "dg")
        enable(texts, "dabcabcg")
        enable(texts, "ddefabcg")
        enable(texts, "ddefdefg")

        for (std::string text : texts){
            std::cout << "Matching '" << text << "' resulted in:\n";
            auto t1 = high_resolution_clock::now();
            const auto answer = root.match(text);
            auto t2 = high_resolution_clock::now();
            std::vector<int> test_result;
            size_t ind = 0;

            for (size_t i = 0 ; i < regexes.size() ; i ++) {
                if (std::regex_search(text, std::regex("^(" + regexes[i] + ")$"))) {
                    test_result.push_back(i + 1);
                }
            }
            auto t3 = high_resolution_clock::now();
            total = total + (t2-t1);
            alternative = alternative + (t3-t2);
            if (answer.size() != test_result.size()) {
                std::cout << "\tFailed:\n";
                for (auto x : answer) {
                    std::cout << "\t\t     My: " << x << ") " << regexes[x - 1] << std::endl;
                }
                std::cout << "\t\t-----------------------\n";
                for (auto x : test_result) {
                    std::cout << "\t\t Actual: " << x << ") " << regexes[x - 1] << std::endl;
                }   
            }
            else {
                bool failed = false;
                for (size_t i = 0 ; i < answer.size() ; i ++) {
                    if (failed = (failed || answer[i] != test_result[i]))
                        std::cout << "\t\t Failed: " << answer[i] << ") " << regexes[answer[i] - 1] << " | " << test_result[i] << ") " << regexes[test_result[i] - 1] << std::endl;
                }
                if (!failed)
                    std::cout << "\t\t Success " << duration<double, std::milli>(t2 - t1).count() << "ms (my) vs " << duration<double, std::milli>(t3 - t2).count() << "ms (O(n^2))" << std::endl;
            }
            //getchar();
        }
        std::cout << "\nTotal " << total.count() << "ms (my) vs " << alternative.count() << "ms (O(n^2))" << std::endl;
    }
    return 0;
}