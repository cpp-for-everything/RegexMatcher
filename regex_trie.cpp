/**
 * @file regex_trie.cpp
 * @author Alex Tsvetanov (alex_tsvetanov_2002@abv.bg)
 * @brief Implement a solution to finding fast if a given string matches with any of the given regexes
 * @version 0.1
 * @date 2023-10-21
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <iostream>
#include <map>
#include <vector>
#include <memory>
#include <stack>
#include <queue>

/**
 * @brief Placeholder class for any information that should be associated with the Regex
 * 
 */
struct Regex {
    std::string regex;
    int uid;
    Regex(std::string regex, int uid) {
        this->regex = regex;
        this->uid = uid;
    }
};

/**
 * @brief Trie-like graph node class
 * 
 */
struct Node {
private:
    /**
     * @brief Contains Regex quantifier information
     * 
     */
    struct RepeatLimits {
        using limit_ptr = int*;
        limit_ptr min;
        limit_ptr max;
        RepeatLimits(limit_ptr min, limit_ptr max) {
            this->min = min;
            this->max = max;
        }
        RepeatLimits() {
            this->min = nullptr;
            this->max = nullptr;
        }
        RepeatLimits(int min, int max) {
            this->min = new int(min);
            this->max = new int(max);
        }
        RepeatLimits(std::nullptr_t, int max) {
            this->min = nullptr;
            this->max = new int(max);
        }
        RepeatLimits(int min, std::nullptr_t) {
            this->min = new int(min);
            this->max = nullptr;
        }
        ~RepeatLimits() {
            if(this->min != nullptr) delete this->min;
            if(this->max != nullptr) delete this->max;
        }
        RepeatLimits* copy() {
            if (min != nullptr && max != nullptr)
                return new RepeatLimits(*min, *max);
            if (min == nullptr && max != nullptr)
                return new RepeatLimits(nullptr, *max);
            if (min != nullptr && max == nullptr)
                return new RepeatLimits(*min, nullptr);
        }
    };
    /**
     * @brief char or wildcard class for the edges of the graph
     * 
     */
    struct char_t {
        char symbol;
        bool wildcard;
        char_t(char symbol) { this->symbol = symbol; this->wildcard = false; }
        char_t() : symbol('\0'), wildcard(true) { }

        bool is_any() const { return symbol == '\0' && wildcard; }
        char get() const { return symbol; }

        bool operator<(const char_t& x) const { return (symbol == x.symbol) ? (wildcard < x.wildcard) : (symbol < x.symbol); }  
        bool operator==(const char_t& x) const { return (symbol == x.symbol) && (wildcard == x.wildcard); }       
    };
    static const char_t any;
    static const char_t placeholder;
public:
    using node_ptr = Node*;
    using ref_ptr = const Regex*;

    std::map<char_t, node_ptr> children; // forward relations
    std::vector<std::pair<node_ptr, RepeatLimits>*> repeaters; // backward relations
    ref_ptr endpoint;
    node_ptr parent;
    
    Node() {
        children = {};
        repeaters = {};
        parent = nullptr;
        endpoint = nullptr;
    }

    ~Node() {
        children.clear();
        repeaters.clear();
        if (parent != nullptr) delete parent;
    }

    void init(const ref_ptr regex_obj) {
        node_ptr leaf = this->init(regex_obj->regex, regex_obj, 0, regex_obj->regex.size(), nullptr, nullptr);
    }

    node_ptr init(const std::string& regex, const ref_ptr ref, size_t start, size_t end, node_ptr parent, node_ptr leaf) {
        this->parent = parent;
        node_ptr current = this;
        if (leaf == nullptr) {
            leaf = new Node();
            leaf->endpoint = ref;
        }
        // temporary elements needed for the capture
            node_ptr last_tmp = nullptr; // root of tmp Node
            node_ptr last_tmp_leaf = nullptr; // leaf of tmp Node
        // temporary elements needed for the repeat
            node_ptr start_of_the_last_repeat = nullptr;
        std::stack<char> brakets;
        size_t substring_start = start, substring_end = end;
        for (size_t i = start ; i < end ; i += 1) {
            if (regex[i] == '(') { // capturing group begins
                // save the end of the capturing group
                size_t local_expression_end = i;

                // find the closing braket
                brakets.push(regex[local_expression_end]);
                while (!brakets.empty()) {
                    local_expression_end ++;
                    if (is_open_braket(regex[local_expression_end])) 
                        brakets.push(regex[local_expression_end]);
                    if (is_close_braket(regex[local_expression_end]))
                        if (is_valid_pair(brakets.top(), regex[local_expression_end]))
                            brakets.pop();
                }

                // parse the capturing group content
                if (local_expression_end != end - 1) { // there's something after the capturing group so don't include it just yet
                    last_tmp = new Node();
                    last_tmp_leaf = new Node();
                    last_tmp->init(regex, nullptr, i + 1, local_expression_end, current, last_tmp_leaf); // skip opening and closing capturing group brakets
                }
                else { // there's nothing after the capturing group so just attach it to the current node
                    return current->init(regex, ref, i + 1, local_expression_end, current, leaf); // skip opening and closing capturing group brakets
                }
                
                // move past the capturing group
                i = local_expression_end;
            }
            else if (regex[i] == '[') { // char set begins
                // add the last_tmp to the current node and move the current node
                if (last_tmp != nullptr) {
                    for(auto x : last_tmp->children) {
                        current->add_child(x.first, x.second);
                    }
                    current = last_tmp_leaf;
                    last_tmp = nullptr;
                }
                // create the node after the end of the char set
                node_ptr after_the_set = new Node();
                after_the_set->parent = current;
                i ++; // skip the braket
                while(regex[i] != ']') {
                    if (regex[i] == '\\') { // next char is to be escaped and treat as normal char of the set
                        i ++;
                        current->add_child(regex[i], after_the_set);
                    }
                    else if (regex[i] == '-') { // range to be added
                        // start from the next of the previous symbol as it was already added from the else below
                        for(auto x = regex[i - 1] + 1 ; x <= regex[i + 1] ; x ++) {
                            current->add_child(x, after_the_set);
                        }
                        i ++; // skip the end of the range as it was added at the last step of the for above
                    }
                    else { // this is a standard symbol and is to be added as a child
                        current->add_child(regex[i], after_the_set);
                    }
                    i ++; // move on
                }
                current = after_the_set;
            }
            else if (regex[i] == '+') {
                if (last_tmp != nullptr) {
                    for(auto x : last_tmp->children) {
                        current->add_child(x.first, x.second);
                    }
                    current = last_tmp_leaf;
                    current->create_repeater(last_tmp, new int(1), nullptr);
                    last_tmp = nullptr;
                }
                else { // the repeat operator is for a char set or a single char
                    current->create_repeater(current->parent, new int(1), nullptr);
                }
                if (i == end - 1) {
                    std::cout << "Is leaf nullptr " << (leaf == nullptr) << std::endl;
                    current->add_child(placeholder, leaf);
                    std::cout << leaf->endpoint->uid << std::endl;
                }
            }
            else if (regex[i] == '*') { }
            else if (regex[i] == '?') { }
            else if (regex[i] == '.') { // wildcard
                if (last_tmp != nullptr) {
                    for(auto x : last_tmp->children) {
                        current->add_child(x.first, x.second);
                    }
                    current = last_tmp_leaf;
                    last_tmp = nullptr;
                }
                node_ptr new_node;
                if (i != end - 1) {
                    new_node = new Node();
                    new_node->parent = current;
                }
                else
                    new_node = leaf;
                current->add_child(Node::any, new_node);
                current = new_node;
            }
            else {
                node_ptr new_node;
                if (i != end - 1) {
                    new_node = new Node();
                    new_node->parent = current;
                }
                else
                    new_node = leaf;
                current->add_child(regex[i], new_node);
                current = new_node;
            }
        }
        return leaf;
    }
    
    void add_child(char_t key, node_ptr next) {
        if (this->children.find(key) != this->children.end()) {
            std::cout << "The key is not new" << std::endl;
            return;
        }
        this->children.emplace(key, next);
    }
    
    void create_repeater(node_ptr return_to, int* a, int* b) {
        int _a = a!=nullptr ? *a : -1;
        int _b = b!=nullptr ? *b : -1;
        std::pair<node_ptr, RepeatLimits>* new_element = new std::pair<node_ptr, RepeatLimits>(return_to, RepeatLimits(a, b));
        this->repeaters.push_back(new_element);
    }
    
    void printDFS(size_t i = 1) {
        for (auto x : children) {
            //if (x.second == nullptr) continue;
            if (!x.first.wildcard && x.first.get() != '\0')
                std::cout << std::string(i, '\t') << x.first.get() << std::endl;
            else if (x.first.get() == '\0') {
                std::cout << std::string(i + 1, '\t') << "(placeholder) " << x.second->endpoint->uid << std::endl;
                x.second->printDFS(i + 2);
            }
            else
                std::cout << std::string(i,'\t') << "<- wildcard" << std::endl;
            for(auto y : x.second->repeaters) {
                for(auto z : y->first->children) {
                    if (!z.first.is_any())
                        std::cout << std::string(i + 1, '\t') << "<-" << z.first.get() << std::endl;
                    else if (z.first == placeholder) {
                        std::cout << std::string(i + 1, '\t') << "(placeholder)" << std::endl;
                        z.second->printDFS(i + 2);
                    }
                    else
                        std::cout << std::string(i + 1, '\t') << "<- wildcard" << std::endl;
                }
            }

            x.second->printDFS(i + 1);
        }
    }
    // BFS algorithm through the graph
    const ref_ptr find(const std::string& text) const {
        //                  <The last matched node, next char to match>
        std::queue<std::pair<node_ptr, size_t>> q;
        //std::map<node_ptr, std::map<node_ptr, std::map<RepeatLimits
        if (auto it = children.find(char_t(text[0])); it != children.end())
            q.push(std::make_pair(it->second, 1));

        if (auto it = children.find(Node::any); it != children.end())
            q.push(std::make_pair(it->second, 1));

        if (auto it = children.find(Node::placeholder); it != children.end())
            q.push(std::make_pair(it->second, 0));
        
        while(!q.empty()) {
            const auto front = q.front(); q.pop();
            const auto front_node = front.first;
            const size_t index_to_find = front.second;

            if (index_to_find == text.size()) { // everything got matched
                if (front_node->endpoint != nullptr)
                    return front_node->endpoint;
                else if (auto it = front_node->children.find(Node::placeholder); it->second->endpoint != nullptr)
                    return it->second->endpoint;
                else 
                    return nullptr;
            }
            
            // TODO: check for minimum repeat count if any
            if (auto it = front_node->children.find(char_t(text[index_to_find])); it != front_node->children.end())
                q.push(std::make_pair(it->second, index_to_find + 1));
            
            if (auto it = front_node->children.find(Node::any); it != front_node->children.end())
                q.push(std::make_pair(it->second, index_to_find + 1));

            if (auto it = front_node->children.find(Node::placeholder); it != front_node->children.end())
                q.push(std::make_pair(it->second, index_to_find));

            // TODO: check for maximum repeat count if any
            for (size_t i = 0 ; i < front_node->repeaters.size() ; i ++) {
                const auto node = front_node->repeaters[i]->first;

                if (const auto it = node->children.find(char_t(text[index_to_find])); it != node->children.end())
                    q.push(std::make_pair(it->second, index_to_find + 1));

                if (const auto it = node->children.find(Node::any); it != node->children.end())
                    q.push(std::make_pair(it->second, index_to_find + 1));
            }
        }
        return nullptr;
    }
private:
    static bool is_open_braket(char x) {
        switch(x) {
            case '(':
            case '{':
            case '[':
                return true;
            default:
                return false;
        }
    }
    static bool is_close_braket(char x) {
        switch(x) {
            case ')':
            case ']':
            case '}':
                return true;
            default:
                return false;
        }
    }
    static bool is_valid_pair(char a, char b) {
        if (a == '(' && b == ')') return true;
        if (a == '[' && b == ']') return true;
        if (a == '{' && b == '}') return true;
        return false;
    }
};

const Node::char_t Node::any = char_t();
const Node::char_t Node::placeholder = char_t('\0');

int main() {
    const Regex* patterns = new Regex[5] {
        Regex("(a)+", 0),
        Regex("a*c", 1),
        Regex("a*b", 2),
        Regex("[a-c]{1,3}", 3),
        Regex("([a-c]+.)", 4)
    };

    Node root0, root4;
    root0.init(&patterns[0]);
    root4.init(&patterns[4]);
    root0.printDFS();
    root4.printDFS();
    std::string text;

    text = "ac";
    if (auto it = root0.find(text); it != nullptr)
        std::cout << patterns[0].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[0].regex << " ^ " << text << " = no match" << std::endl; 

    text = "acc";
    if (auto it = root0.find(text); it != nullptr)
        std::cout << patterns[0].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[0].regex << " ^ " << text << " = no match" << std::endl; 

    text = "aac";
    if (auto it = root0.find(text); it != nullptr)
        std::cout << patterns[0].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[0].regex << " ^ " << text << " = no match" << std::endl; 

    text = "aaaa";
    if (auto it = root0.find(text); it != nullptr)
        std::cout << patterns[0].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[0].regex << " ^ " << text << " = no match" << std::endl; 

    text = "ac";
    if (auto it = root4.find(text); it != nullptr)
        std::cout << patterns[4].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[4].regex << " ^ " << text << " = no match" << std::endl; 

    text = "acc";
    if (auto it = root4.find(text); it != nullptr)
        std::cout << patterns[4].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[4].regex << " ^ " << text << " = no match" << std::endl; 

    text = "aac";
    if (auto it = root4.find(text); it != nullptr)
        std::cout << patterns[4].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[4].regex << " ^ " << text << " = no match" << std::endl; 
}

/*
Single regex operations:
    wildcard - done
    captures - done
    sets     - done
    +        - done
    *
    ?
    |
Multiple regex operations:
    merge 2 nodes
*/