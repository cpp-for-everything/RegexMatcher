#include <RegexMatcher.hpp>
#include <iostream>

#define enable(x,y) x.push_back(y);
#define disable(x,y)

int main(int argc, char** argv) {
    {
        RegexMatcher<int, char> root;
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

        for (std::string regex : regexes) {
            std::cout << "'" << regex << "' is number " << ++num << std::endl;
            auto it = regex.begin();
            root.add_regex(regex, num);
        }
        root.print_list_of_edges();

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
            std::cout << "Matching '" << text << "' resulted in: ";
            for (auto x : root.match(text)) std::cout << x << " ";
            std::cout << std::endl;
        }
    }
    return 0;
}