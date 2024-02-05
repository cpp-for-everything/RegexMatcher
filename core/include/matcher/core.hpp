#pragma once

#include "RegexMatcherConfig.h"

#include <set>
#include <map>
#include <list>
#include <string>
#include <vector>
#include <optional>
#include <sstream>

#ifdef DEBUG
#include <iostream>
#endif

/**
 * @brief Public classes of the library
 * 
 */
namespace matcher {
    /**
     * @brief Contains the interface for adding regexes and matching a string with the trie 
     * 
     * @tparam RegexData Type of associated data with each regex
     * @tparam char_t Type of the indiviidual symbols in the strings and regexes
     */
    template<typename RegexData, typename char_t = char>
    class RegexMatcher;
}

/**
 * @brief Private helper functions and classes for the library
 * 
 */
namespace {

    /**
     * @brief Class containing the min and max repeat of the same edge
     * 
     */
    struct Limits;
    
    /**
     * @brief Class containing the list of regexes using the given edge
     * 
     * @tparam T type of the reference to the match data
     */
    template<typename T, typename Node>
    struct EdgeInfo;

    /**
     * @brief Contains the characters and additional attributes for wildcard symbols and Null symbols
     * 
     */
    template<typename char_t>
    struct symbol;

    /**
     * @brief Node in the trie-grah
     * 
     * @tparam RegexData Type of the data associated with each regex
     * @tparam char_t Type of symbols used
     */
    template<typename RegexData, typename char_t>
    class Node;

    template<typename Node_T>
    class SubTree;
}

namespace {
    struct Limits {
        size_t min;
        std::optional<size_t> max;
        
        Limits() : Limits (0, std::nullopt) {}
        
        Limits(size_t min, std::nullopt_t) {
            this->min = min;
            this->max = std::nullopt;
        }
        
        Limits(std::nullopt_t, size_t max) : Limits (0, max) {}
        
        Limits(size_t min, size_t max) {
            this->min = min;
            this->max = max;
        }

        static const Limits common_edge;
        static const Limits zero_or_once;
        static const Limits once_or_more;
        static const Limits zero_or_more;

        static std::string to_string(std::optional<std::list<Limits>::iterator> it) {
            if (!it.has_value()) { return ""; }
            std::stringstream ss;
            if (!it.value()->max.has_value()) { ss << "(" << it.value()->min << "+)"; }
            else { ss << "(" << it.value()->min << "..." << it.value()->max.value() << ")"; }
            return ss.str();
        }

        static std::string to_string(Limits a) {
            std::stringstream ss;
            if (!a.max.has_value()) { ss << "(" << a.min << "+)"; }
            else { ss << "(" << a.min << "..." << a.max.value() << ")"; }
            return ss.str();
        }

        Limits& operator--() {
            if (min > 0) min --;
            if (max.has_value() && max.value() > 0) max = max.value() - 1;
            return *this;
        }

        bool is_required() const {
            return min > 0;
        }

        bool is_allowed_to_repeat() const {
            return !max.has_value() || (max.value() > 0 && this->min <= this->max.value());
        }
    };

    const Limits Limits::common_edge = Limits();
    const Limits Limits::zero_or_once = Limits(0, 1);
    const Limits Limits::once_or_more = Limits(1, std::nullopt);
    const Limits Limits::zero_or_more = Limits(0, std::nullopt);

    template<typename T, typename Node>
    struct EdgeInfo {
        std::map<T, std::optional<std::list<Limits>::iterator>> paths; // each path may have different requirements for how many times should the edge be repeated.
        Node* to;
    };

    template<typename char_t>
    struct symbol {
        char_t ch;
        bool wildcard;
        bool none;

        static const symbol Any;
        static const symbol None;
        static const symbol EOR; // end-of-regex

        symbol() : symbol(symbol<char_t>::None) {}
        symbol(char_t s) : ch(s), wildcard(false), none(false) {}
        symbol(char_t s, bool w, bool n) : ch(s), wildcard(w), none(n) {}

        inline bool operator== (const symbol& s) const {
            return (wildcard == s.wildcard) && (none == s.none) && (ch == s.ch);
        }
        inline bool operator!= (const symbol<char_t>& s) const {
            return (wildcard != s.wildcard) || (none != s.none) || (ch != s.ch);
        }

        bool operator<(const symbol<char_t>& s) const {
            if (ch == s.ch) {
                if (wildcard == s.wildcard)
                    return none < s.none;
                return wildcard < s.wildcard;
            }
            return (ch < s.ch);
        }

        inline std::basic_string<char_t> to_string() const {
            if (*this == symbol::Any) return "wildcard";
            if (*this == symbol::None) return "(empty)";
            if (*this == symbol::EOR) return "EOR";
            return std::basic_string<char_t>(1, ch);
        }
    };

    template<typename char_t>
    const symbol<char_t> symbol<char_t>::Any{'\0', true, false};

    template<typename char_t>
    const symbol<char_t> symbol<char_t>::None{'-', false, true};

    template<typename char_t>
    const symbol<char_t> symbol<char_t>::EOR{'\0', false, true};

    template<typename RegexData, typename char_t>
    class Node {
    private:
        static std::map<symbol<char_t>, std::string> special_symbols;
        static std::list<Limits> all_limits;

        /**
         * @brief All directly connected nodes
         * 
         */
        std::map<symbol<char_t>, EdgeInfo<RegexData, Node<RegexData, char_t>>> neighbours;

        /**
         * @brief The current symbol that the regex would match
         * 
         */
        symbol<char_t> current_symbol;

        /**
         * @brief Construct a new Node object
         * 
         */
        Node () {
            current_symbol = symbol<char_t>::None;
        }

        /**
         * @brief Construct a new Node object
         * 
         * @param ch Node's symbol<char_t>
         */
        Node (symbol<char_t> ch) {
            current_symbol = ch;
        }

        /**
         * @brief Represents the current node's symbol as string
         * 
         * @return std::string string representation of the node's symbol<char_t>
         */
        std::string to_string() const {
            if (current_symbol == symbol<char_t>::Any)
                return "wildcard";
            if (current_symbol == symbol<char_t>::None)
                return "(empty)";
            return std::string(1, current_symbol.ch);
        }

        /**
         * @brief Checks if there is a node with a given symbol in the neighbour list  
         * 
         * @param ch Symbol to be checked
         * @return true if a node with this symbol is direct neighbour to this node
         * @return false if there is no node with this symbol as direct neighbour to this node
         */
        bool hasChild(symbol<char_t> ch);

        /**
         * @brief Get the Child object with the given symbol
         * 
         * @param ch the symbol that's being looked for
         * @return Node* the node correspondign to this symbol
         */
        Node<RegexData, char_t>* getChild(symbol<char_t> ch);

        /**
         * @brief Adds a child node to the current one and marks the connection as part of a given regex match
         * 
         * @param child  Existing node
         * @param regex  Regex data that is being used to indentify the regex that the edge is part of
         * @param limits Pointer to the shared limit of the edge (nullptr if no limit is applied)
         */
        void connect_with(Node<RegexData, char_t>* child, RegexData regex, std::optional<std::list<Limits>::iterator> limits = std::nullopt);

        /**
         * @brief Matches a string with all regexes and returns the identified of the one that matches
         * 
         * @param text string that is being tried to be matched with any of the added regexes
         * @tparam ConstIterator const iterator in a container
         * @return std::vector<RegexData> set of unique identifiers of the regexes that matches the string
         */
        template<typename ConstIterator>
        std::vector<RegexData> match(ConstIterator, ConstIterator);

        template<typename ConstIterator>
        std::vector<RegexData> match_helper(ConstIterator, ConstIterator, const std::vector<RegexData>&, Node*);

    #ifdef DEBUG
        void print_helper(size_t layer, std::set<const Node<RegexData, char_t>*>& traversed, std::map<const Node<RegexData, char_t>*, std::string>& nodes) const;
        
        void print() const;
    #endif

        friend class matcher::RegexMatcher<RegexData, char_t>;
    };

    template<typename Node_T>
    class SubTree {
    public:
        std::vector<Node_T*> roots;
        std::vector<Node_T*> leafs;

        SubTree(std::vector<Node_T*> a, std::vector<Node_T*> b) : roots(a), leafs(b) {}
        SubTree() : roots(), leafs() {}

        inline const std::vector<Node_T*>& get_roots() const;

        inline const std::vector<Node_T*>& get_leafs() const;
    };
}

#include <matcher/impl/subtree.cpp>
#include <matcher/impl/node.cpp>

namespace matcher {
    template<typename RegexData, typename char_t>
    class RegexMatcher {
        Node<RegexData, char_t> root;

        template<typename ConstIterator>
        static std::list<Limits>::iterator processLimit(const SubTree<Node<RegexData, char_t>>&, SubTree<Node<RegexData, char_t>>&, RegexData, ConstIterator&);
        
        template<typename ConstIterator>
        static SubTree<Node<RegexData, char_t>> processSet(std::vector<Node<RegexData, char_t>*>, RegexData, ConstIterator&);
        
        template<typename ConstIterator>
        static SubTree<Node<RegexData, char_t>> process(std::vector<Node<RegexData, char_t>*>, RegexData, ConstIterator&, ConstIterator, const bool);

    public:
        /**
         * @brief Construct a new Regex Matcher object
         * 
         */
        RegexMatcher() {}

        /**
         * @brief Adds another regex to the set of regexes
         * 
         * @tparam Iterable Set of characters for the regex. Must implement std::cbegin and std::cend
         * @tparam RegexData Value that will be associated with the regex
         */
        template<typename Iterable>
        void add_regex(Iterable, RegexData);

        /**
         * @brief Matches a string with all added regexes
         * 
         * @tparam Iterable Set of characters. Must implement std::cbegin and std::cend
         * @return std::vector<RegexData> Set of regexes' UIDs that match the text
         */
        template<typename Iterable>
        std::vector<RegexData> match(Iterable);

    #ifdef DEBUG
        /**
         * @brief Prints list of edges withing the pattern graph
         * 
         */
        void print() const {
            this->root.print();
        }
    #endif
    };
}

#include <matcher/impl/core.cpp>