#include "../lib/node.hpp"
#include <iostream>

#define enable(x,y) x.push_back(y);
#define disable(x,y)

int main(int argc, char** argv) {
    {
        Node root = Node();
        int num = 0;
        std::vector<std::string> regexes;
        
        enable(regexes, "d(ab|cd)*g+")
        disable(regexes, "a?")
        disable(regexes, "b|c")
        disable(regexes, "(d|e)f")
        disable(regexes, "f[a-c]?d(ab|cd)*g+")

        for (std::string regex : regexes) {
            std::cout << "'" << regex << "' is number " << ++num << std::endl;
            auto it = regex.begin();
            process({&root}, num, it, regex.end(), false);
        }
        root.print();

        std::vector<std::string> texts;

        disable(texts, "")
        disable(texts, "a")
        disable(texts, "b")
        disable(texts, "c")
        disable(texts, "d")
        disable(texts, "df")
        disable(texts, "e")
        disable(texts, "ef")
        disable(texts, "fdg")
        disable(texts, "fdgg")
        disable(texts, "fd")

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
        enable(texts, "dabab")
        enable(texts, "dabcd")
        enable(texts, "dcdcd")
        enable(texts, "dabg")
        enable(texts, "dg")
        enable(texts, "dababg")
        enable(texts, "dcdabg")
        enable(texts, "dcdcdg")

        for (std::string text : texts){
            std::cout << "Matching '" << text << "' resulted in: ";
            for (auto x : root.match(text)) std::cout << x << " ";
            std::cout << std::endl;
        }
    }
    return 0;
}