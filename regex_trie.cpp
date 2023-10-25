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
#include <optional>
#include <functional>

#ifdef VERBOSE_DEBUG 
    static constexpr bool VERBOSE = true;
#else
    static constexpr bool VERBOSE = false;
#endif

#define debug if constexpr (VERBOSE) std::cout

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
        using limit_ptr = std::optional<size_t>;
        limit_ptr min;
        limit_ptr max;
        RepeatLimits(limit_ptr min, limit_ptr max) {
            this->min = min;
            this->max = max;
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
        std::string print() const {
            if (wildcard && symbol == '\0') {
                return "wildcard";
            }
            if (!wildcard && symbol == '\0') {
                return "\\0";
            }
            return std::string(1, symbol);
        }

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
        std::string regex = "(" + regex_obj->regex + ")";
        debug << "reworked regex " << regex << std::endl;
        node_ptr leaf = this->init(regex, regex_obj, 0, regex.size(), nullptr, nullptr);
    }

    node_ptr init(const std::string& regex, const ref_ptr ref, size_t start, size_t end, node_ptr parent, node_ptr leaf) {
        this->parent = parent;
        node_ptr current = this;
        if (leaf == nullptr) {
            leaf = new Node();
            leaf->endpoint = ref;
        }
        // temporary elements needed for the capture
        node_ptr last_tmp = nullptr; // root of capture Node
        node_ptr last_tmp_leaf = nullptr; // leaf of capture Node

        const auto attach_tmp_to_current = [&](std::function<void()> if_tmp = [](){}, std::function<void()> if_not_tmp = [](){}) {
            if (last_tmp != nullptr) {
                for(auto x : last_tmp->children) {
                    current->add_child(x.first, x.second);
                }
                current = last_tmp_leaf;
                if_tmp();
                last_tmp = nullptr;
                last_tmp_leaf = nullptr;
            }
            else {
                if_not_tmp();
            }
        };
        std::stack<char> brakets;
        for (size_t i = start ; i < end ; i += 1) {
            if (regex[i] == '(') { // capturing group begins
                debug << "Starting a capture at " << i << std::endl;
                if (last_tmp != nullptr) {
                    for(auto x : last_tmp->children) {
                        current->add_child(x.first, x.second);
                    }
                    current = last_tmp_leaf;
                    last_tmp = nullptr;
                    last_tmp_leaf = nullptr;
                }

                last_tmp = new Node();
                last_tmp_leaf = new Node();
                    
                // save the end of the capturing group
                size_t local_expression_end = i;
                size_t substring_start = i + 1;
                std::vector<std::pair<size_t, size_t>> subregexes_in_between_ORs;

                // find the closing braket
                brakets.push(regex[local_expression_end]);
                while (!brakets.empty()) {
                    local_expression_end ++;
                    if (is_open_braket(regex[local_expression_end])) 
                        brakets.push(regex[local_expression_end]);
                    if (is_close_braket(regex[local_expression_end]))
                        if (is_valid_pair(brakets.top(), regex[local_expression_end]))
                            brakets.pop();
                    if(regex[local_expression_end] == '|') {
                        debug << "subregex: " << regex.substr(substring_start, local_expression_end - substring_start) << std::endl;
                        subregexes_in_between_ORs.push_back({substring_start, local_expression_end});
                        substring_start = local_expression_end + 1;
                    }
                }
                if(substring_start != i + 1) {
                    debug << "subregex: " << regex.substr(substring_start, local_expression_end - substring_start) << std::endl;
                    subregexes_in_between_ORs.push_back({substring_start, local_expression_end});
                    substring_start = local_expression_end + 1;
                }
                else {
                    subregexes_in_between_ORs.push_back({substring_start, local_expression_end});
                }

                // parse the capturing group content
                if (local_expression_end != end - 1) { // there's nothing after the capturing group so just attach it to the current node
                    for (auto ranges : subregexes_in_between_ORs) {
                        last_tmp->init(regex, nullptr, ranges.first, ranges.second, current, last_tmp_leaf);
                    }
                }
                else {
                    for (auto ranges : subregexes_in_between_ORs) {
                        current->init(regex, ref, ranges.first, ranges.second, current->parent, leaf);
                    }
                    current = leaf;
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
                    last_tmp_leaf = nullptr;
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
                    current->create_repeater(last_tmp, 1, std::nullopt);
                    last_tmp = nullptr;
                    last_tmp_leaf = nullptr;
                }
                else {
                    current->create_repeater(current->parent, 1, std::nullopt);
                }

                if (i == end - 1) {
                    debug << "Is leaf nullptr " << (leaf == nullptr) << std::endl;
                    current->add_child(placeholder, leaf);
                    debug << leaf->endpoint->uid << std::endl;
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
                    last_tmp_leaf = nullptr;
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
                if (last_tmp != nullptr) {
                    for(auto x : last_tmp->children) {
                        current->add_child(x.first, x.second);
                    }
                    current = last_tmp_leaf;
                    last_tmp = nullptr;
                    last_tmp_leaf = nullptr;
                }

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
            debug << "The key is not new" << std::endl;
            return;
        }
        this->children.emplace(key, next);
    }
    
    void create_repeater(node_ptr return_to, RepeatLimits::limit_ptr a, RepeatLimits::limit_ptr b) {
        std::pair<node_ptr, RepeatLimits>* new_element;
        if (a.has_value() && b.has_value())
            new_element = new std::pair<node_ptr, RepeatLimits>(return_to, RepeatLimits(a.value(), b.value() - 1));
        else if (a.has_value() && !b.has_value())
            new_element = new std::pair<node_ptr, RepeatLimits>(return_to, RepeatLimits(a.value(), b));
        else if (!a.has_value() && b.has_value())
            new_element = new std::pair<node_ptr, RepeatLimits>(return_to, RepeatLimits(a, b.value() - 1));
        this->repeaters.push_back(new_element);
    }
    
    void printDFS(size_t i = 1) const {
        std::cout << std::string(i, '\t') << "Node " << this;
        if (this->endpoint != nullptr) 
            std::cout << "(Endpoint for " << this->endpoint->uid << ")";
        std::cout << std::endl;
        for (auto x : children) {
            if (!x.first.wildcard && x.first.get() != '\0') {
                std::cout << std::string(i + 1, '\t') << "--" << x.first.get() << "-->" << std::endl;
            }
            else if (x.first.get() == '\0' && !x.first.wildcard) {
                std::cout << std::string(i + 1, '\t') << "--\\0--> " << x.second->endpoint->uid << std::endl;
            }
            else {
                std::cout << std::string(i + 1, '\t') << "--wildcard-->" << std::endl;
            }
            
            x.second->printDFS(i+2);
        }
        for(auto y : this->repeaters) {
            std::cout << std::string(i + 1, '\t') << "<--return to--" << y->first << " (min: " << y->second.min.value_or(0) << ", max: " << y->second.max.value_or(-1) << ")" << std::endl;
        }
    }
    // BFS algorithm through the graph
    const ref_ptr find(const std::string& text) {
        //this->printDFS(0);
        std::map<std::pair<node_ptr, node_ptr>, RepeatLimits> limits_to_satisfy;
        auto answer = find_helper(text, 0, this->repeaters.size() == 0);
        return (answer == nullptr) ? nullptr : answer->endpoint;
    }
    node_ptr find_helper(const std::string& text, size_t current_index, bool repeat_not_a_must, int cap = 0) {
        debug << std::string(cap, '\t') << this << "[current_index = " << current_index << ", size = " << text.size() << ", repeat_not_a_must=" << repeat_not_a_must << "]" << std::endl;
        if (current_index == text.size()) {
            debug << std::string(cap, '\t') << "Children: ";
            for (auto x : this->children) 
                debug << x.first.print() << " ";
            debug << std::endl;

            if (auto it = this->children.find(Node::placeholder); it != this->children.end() && it->second->endpoint != nullptr) {
                debug << std::string(cap, '\t') << "  Placeholder point to an answer: " << it->second->endpoint->uid << std::endl;
                return it->second;
            }
            else {
                for (size_t i = 0 ; i < this->repeaters.size() ; i ++) {
                    if(!this->repeaters[i]->second.min.has_value() || this->repeaters[i]->second.min.value() == 0)
                        if (this->repeaters[i]->first->endpoint != nullptr) {
                            debug << std::string(cap, '\t') << "  Repeater points us to the answer: " << this->repeaters[i]->first->endpoint->uid << std::endl;
                            return this->repeaters[i]->first;
                        }
                }

                debug << std::string(cap, '\t') << "  Looking at this: ";
                if (this->endpoint != nullptr) {
                    debug << this->endpoint->uid << std::endl;
                }
                else
                    debug << "no endpoint here" << std::endl;

                return this;
            }
        }
        if (repeat_not_a_must) { // try moving forward directly without repeaters
            if (auto it = children.find(char_t(text[current_index])); it != children.end()) { // we found the current char directly
                debug << std::string(cap + 1, '\t') << "--" << it->first.get() << "-->" << it->second << std::endl;
                if (auto res = it->second->find_helper(text, current_index + 1, repeat_not_a_must, cap + 1); res != nullptr && res->endpoint != nullptr)
                    return res;
            }

            if (auto it = children.find(Node::any); it != children.end()) { // we found the current char as wildcard
                debug << std::string(cap + 1, '\t') << "--" << "wildcard" << "-->" << it->second << std::endl;
                if (auto res = it->second->find_helper(text, current_index + 1, repeat_not_a_must, cap + 1); res != nullptr && res->endpoint != nullptr)
                    return res;
            }

            if (auto it = children.find(Node::placeholder); it != children.end()) { // we found '\0' which is a placeholder root of subtree so let's try this subtree
                debug << std::string(cap + 1, '\t') << "--" << "\\0" << "-->" << it->second << std::endl;
                if (auto res = it->second->find_helper(text, current_index, repeat_not_a_must, cap + 1); res != nullptr && res->endpoint != nullptr)
                    return res;
            }
        }
        
        { // try moving forward with repeats
            for (size_t i = 0 ; i < this->repeaters.size() ; i ++) {
                const auto node = this->repeaters[i]->first;

                node_ptr tmp = node;
                const Node::RepeatLimits::limit_ptr min = this->repeaters[i]->second.min; // min is 0 by default
                const Node::RepeatLimits::limit_ptr max = this->repeaters[i]->second.max; // max is inf by default

                debug << std::string(cap + 1, '\t') << "<--" << this->repeaters[i]->first << "-- with [" << min.value_or(0) << ", " << max.value_or((size_t)-1) << ")" << std::endl;

                const auto decrease_if_exists = [](Node::RepeatLimits::limit_ptr& optional) -> void {
                    if (optional.has_value() && optional.value() > 0) 
                        optional = optional.value() - 1;
                };

                const auto recover_the_limits = [&]() {
                    this->repeaters[i]->second.min = min;
                    this->repeaters[i]->second.max = max;    
                };

                if (min.value_or(0) > 0) { // we must repeat the repeater
                    decrease_if_exists(this->repeaters[i]->second.min);
                    decrease_if_exists(this->repeaters[i]->second.max);
                    const bool is_min_requirement_satisfied = this->repeaters[i]->second.min.value_or(0) == 0;
                    if (auto repeat_res = tmp->find_helper(text, current_index, is_min_requirement_satisfied, cap + 1); repeat_res != nullptr && repeat_res->endpoint != nullptr) {
                        recover_the_limits();
                        return repeat_res;
                    }
                    recover_the_limits();
                }
                else if (min.value_or(0) == 0 && max.value_or((size_t)-1) > 0) { // reached the minimum requirement but not the maximum
                    decrease_if_exists(this->repeaters[i]->second.max);
                    if (auto repeat_res = tmp->find_helper(text, current_index, true, cap + 1); repeat_res != nullptr && repeat_res->endpoint != nullptr) {
                        recover_the_limits();
                        return repeat_res;
                    }
                    recover_the_limits();
                }
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
        Regex("a|b", 0),
        Regex("ba+c", 1),
        Regex("a*b", 2),
        Regex("[a-c]{1,3}", 3),
        Regex("([a-c]+.)", 4)
    };

    Node root[5];
    for (size_t i = 0 ; i < 5 ; i ++) {
        if(i != 0 && i != 1 && i != 4) continue;
        root[i].init(&patterns[i]);
        std::cout << patterns[i].regex << ":" << std::endl;
        root[i].printDFS();
        std::cout << "--------------------------" << std::endl;
    }

    std::string text;

    text = "a";
    if (auto it = root[0].find(text); it != nullptr)
        std::cout << patterns[0].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[0].regex << " ^ " << text << " = no match" << std::endl; 

    text = "b";
    if (auto it = root[0].find(text); it != nullptr)
        std::cout << patterns[0].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[0].regex << " ^ " << text << " = no match" << std::endl; 

    text = "ab";
    if (auto it = root[0].find(text); it != nullptr)
        std::cout << patterns[0].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[0].regex << " ^ " << text << " = no match" << std::endl; 

    text = "ac";
    if (auto it = root[0].find(text); it != nullptr)
        std::cout << patterns[0].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[0].regex << " ^ " << text << " = no match" << std::endl; 

    text = "acc";
    if (auto it = root[0].find(text); it != nullptr)
        std::cout << patterns[0].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[0].regex << " ^ " << text << " = no match" << std::endl; 

    text = "aac";
    if (auto it = root[0].find(text); it != nullptr)
        std::cout << patterns[0].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[0].regex << " ^ " << text << " = no match" << std::endl; 

    text = "aaaa";
    if (auto it = root[0].find(text); it != nullptr)
        std::cout << patterns[0].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[0].regex << " ^ " << text << " = no match" << std::endl; 
    
    text = "";
    if (auto it = root[0].find(text); it != nullptr)
        std::cout << patterns[0].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[0].regex << " ^ " << text << " = no match" << std::endl; 
    
    text = "b";
    for (int i = 0 ; i < 5 ; i ++)
    {
        text += "a";
        if (auto it = root[1].find(text + "c"); it != nullptr)
            std::cout << patterns[1].regex << " ^ " << text << "c = match" << std::endl; 
        else
            std::cout << patterns[1].regex << " ^ " << text << "c = no match" << std::endl; 
    }

    text = "ac";
    if (auto it = root[4].find(text); it != nullptr)
        std::cout << patterns[4].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[4].regex << " ^ " << text << " = no match" << std::endl; 

    text = "acc";
    if (auto it = root[4].find(text); it != nullptr)
        std::cout << patterns[4].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[4].regex << " ^ " << text << " = no match" << std::endl; 

    text = "aac";
    if (auto it = root[4].find(text); it != nullptr)
        std::cout << patterns[4].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[4].regex << " ^ " << text << " = no match" << std::endl; 

    text = "aab";
    if (auto it = root[4].find(text); it != nullptr)
        std::cout << patterns[4].regex << " ^ " << text << " = match" << std::endl; 
    else
        std::cout << patterns[4].regex << " ^ " << text << " = no match" << std::endl; 

    delete[] patterns;
}

/*
Single regex operations:
    RepeatLimiters - done
    wildcard       - done
    captures       - done
    sets           - done
    +              - done
    *
    ?
    |
Multiple regex operations:
    merge 2 nodes
*/