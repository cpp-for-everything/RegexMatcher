#include "../lib/node.hpp"
#include <iostream>

#define enable if(true)
#define disable if(false)

int main(int argc, char** argv) {
    {
        Node root = Node();
        int num = 0;
        enable { // startup
            std::string regex =  "a";
            auto it = regex.begin();
            process({&root}, ++num, it, regex.end(), false);
        }
        disable { // sets and brackets
            std::string regex =  "f[a-c]d(asdf)g";
            auto it = regex.begin();
            process({&root}, ++num, it, regex.end(), false);
        }
        disable { // sets and brackets with overlapping with previous regex
            std::string regex = "f[c-d]d(asd)h";
            auto it = regex.begin();
            process({&root}, ++num, it, regex.end(), false);
        }
        disable { // brackets in brackets with ORs
            std::string regex = "f((c|d)d(a|b))e";
            auto it = regex.begin();
            process({&root}, ++num, it, regex.end(), false);
        }
        disable { // OR operator
            std::string regex = "a(bc|de)f";
            auto it = regex.begin();
            process({&root}, ++num, it, regex.end(), false);
        }
        disable { // Repeat operator
            std::string regex = "a(b|c){1,3}";
            auto it = regex.begin();
            process({&root}, ++num, it, regex.end(), false);
        }
        disable { // Repeat operator
            std::string regex = "a(b|c){1,}";
            auto it = regex.begin();
            process({&root}, ++num, it, regex.end(), false);
        }
        enable { // Repeat operator
            std::string regex = "a(b|c){,3}d";
            auto it = regex.begin();
            process({&root}, ++num, it, regex.end(), false);
        }
        enable { // Repeat operator
            std::string regex = "a.{,3}d";
            auto it = regex.begin();
            process({&root}, ++num, it, regex.end(), false);
        }
        root.print();
    }
    return 0;
}