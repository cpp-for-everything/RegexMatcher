#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <list>
#include <optional>

struct limit
{
    int min;
    std::optional<int> max;

    constexpr limit(int min, int max) : min(min), max(max) { }
    constexpr limit(int min) : min(min), max(std::nullopt) { }

    static const limit once;
    static const limit at_most_once;
    static const limit at_least_once;
};

static const limit once = limit(1, 1);
static const limit at_most_once = limit(0, 1);
static const limit at_least_once = limit(1);

struct edge_manager
{
    using edge_limit_ptr = std::list<limit>::iterator;
    std::list<limit> limits;

    edge_limit_ptr prepare_edge_limit(const limit& x)
    {
        return limits.insert(limits.end(), x);
    }

    struct matcher_edge_matcher
    {
        struct visits {
            bool can_visit_again = true;
            int counts = 0;
        };
        std::map<edge_limit_ptr, visits> visited;
        const edge_manager& manager;
        matcher_edge_matcher(const edge_manager& manager) : manager(manager), visited() {}
        matcher_edge_matcher(matcher_edge_matcher&& matcher_manager) : manager(matcher_manager.manager), visited(std::move(matcher_manager.visited)) {}
        
    };


};

struct interval
{
    char from, to;
};

struct node;

struct chidren_set
{
    std::vector<std::pair<node*, edge_manager::edge_limit_ptr>> children;

};

struct node {
    interval current;
    chidren_set children;
};

node* createTreeFrom(std::string str) {

}

int main()
{
    std::cout << "Hello world" << std::endl;
    return 0;
}