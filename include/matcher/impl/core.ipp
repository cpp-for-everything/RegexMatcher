/**
 * @file core.ipp
 * @brief Template implementation for RegexMatcher class
 * @note This file is included by core.hpp - do not include directly
 */

#include <list>

namespace matcher {

	template <typename RegexData, typename char_t>
	template <typename ConstIterator>
	Limits* RegexMatcher<RegexData, char_t>::processLimit(const SubTree<Node<RegexData, char_t>>& parent_of_latest,
	                                                      SubTree<Node<RegexData, char_t>>& lastest, RegexData regex,
	                                                      ConstIterator& it,
	                                                      std::vector<std::unique_ptr<Limits>>& limits_storage) {
		if (*it != '{')  // not called at the beginning of a set
		{
			throw std::logic_error("The iterator doesn't start from a limit group.");
		} else {
			it++;
		}

		auto answer_unique = std::make_unique<Limits>(Limits::common_edge);
		auto answer = answer_unique.get();
		answer->from_quantifier = true;
		limits_storage.push_back(std::move(answer_unique));

		bool min = true;
		size_t number = 0;

		number = 0;
		while (*it != '}') {
			if (*it == ',') {
				min = false;
				answer->min = number;
				number = 0;
			} else {  // it is a digit
				number = number * 10 + (*it - '0');
			}
			it++;
		}

		if (!min && number != 0) {
			answer->max = number;
		}
		if (!min && number == 0) {
			answer->max = std::nullopt;
		}
		if (min) {
			// {n}, with no comma. This set the upper bound and left the lower one at the
			// zero it was constructed with, so {3} meant "up to three" and a[0-9]{3}z
			// matched az. Both bounds are the same number in this form.
			answer->min = number;
			answer->max = number;
		}

		const size_t leafs = lastest.get_leafs().size();

		if (answer->min == 0) {
			for (auto root : parent_of_latest.get_leafs()) {
				lastest.leafs.push_back(root);
			}
			answer->min = 1;
		}
		answer->min = answer->min - 1;
		if (answer->max.has_value()) {
			answer->max = answer->max.value() - 1;
		}

		if (answer->is_allowed_to_repeat()) {
			for (size_t i = 0; i < leafs; i++) {
				for (auto root : lastest.get_roots()) {
					// if another child with same symbol exists
					if (lastest.get_leafs()[i]->hasChild(root->current_symbol) &&
					    lastest.get_leafs()[i]->neighbours[root->current_symbol].to != root) {
						Node<RegexData, char_t>* old_child =
						    lastest.get_leafs()[i]->neighbours[root->current_symbol].to;
						root->absorb(old_child);
						lastest.get_leafs()[i]->neighbours[root->current_symbol].to = root;
					}
					lastest.get_leafs()[i]->connect_with(root, regex, limits_storage, answer);
				}
			}
		}

		return answer;
	}

	template <typename RegexData, typename char_t>
	template <typename ConstIterator>
	SubTree<Node<RegexData, char_t>> RegexMatcher<RegexData, char_t>::processSet(
	    std::vector<Node<RegexData, char_t>*> parents, [[maybe_unused]] RegexData regex, ConstIterator& it,
	    std::deque<Node<RegexData, char_t>>& nodes_storage, std::uint32_t& class_counter) {
		if (*it != '[')  // not called at the beginning of a set
		{
			throw std::logic_error("The iterator doesn't start from a set group.");
		} else {
			it++;
		}
		// One node for the whole class, carrying its members, rather than one node per
		// member. A node's identity is what it consumes, and with a member per node a
		// repeat over the class has to connect every member to every member: for the
		// 66-character class a route parameter uses, that is 66 nodes and 4356 edges,
		// per pattern. As one node it is one node and one self-edge.
		std::vector<char_t> members;
		ConstIterator prev;
		bool takeTheNextSymbolLitterally = false;
		while (*it != ']' || takeTheNextSymbolLitterally) {
			if (!takeTheNextSymbolLitterally) {
				if (*it == '\\') {  // escape symbol is always followed by a reglar character
					it++;           // so it is included no matter what
					takeTheNextSymbolLitterally = true;
				} else if (*it == '-') {
					it++;
					for (char_t ch = ((*prev) + 1); ch <= *it; ch++) {
						members.push_back(ch);
					}
				}
				// TODO: implement not
				else {
					takeTheNextSymbolLitterally = true;
				}
			}
			if (takeTheNextSymbolLitterally) {
				members.push_back(*it);
				takeTheNextSymbolLitterally = false;
			}
			prev = it;
			it++;
		}

		std::sort(members.begin(), members.end());
		members.erase(std::unique(members.begin(), members.end()), members.end());

		// Reuse a class node one of the parents already leads to with exactly these
		// members, on the same reasoning the per-character version reused a child: two
		// patterns sharing a prefix and a class should share the state, not duplicate
		// it.
		Node<RegexData, char_t>* node = nullptr;
		for (auto parent : parents) {
			for (const auto& [sym, edge] : parent->neighbours) {
				if (sym.is_class() && edge.to != nullptr && edge.to->class_members() == members) {
					node = edge.to;
					break;
				}
			}
			if (node != nullptr) {
				break;
			}
		}

		if (node == nullptr) {
			// Ids start at one; zero means "not a class". Each occurrence gets its own,
			// so two different classes leaving one node remain two edges.
			nodes_storage.emplace_back(symbol<char_t>::of_class(++class_counter));
			node = &nodes_storage.back();
			node->ensure_extras().members = std::move(members);
		}

		std::vector<Node<RegexData, char_t>*> leafs{node};
		return {leafs, leafs};
	}

	template <typename RegexData, typename char_t>
	template <typename ConstIterator>
	SubTree<Node<RegexData, char_t>> RegexMatcher<RegexData, char_t>::process(
	    std::vector<Node<RegexData, char_t>*> parents, RegexData regex, ConstIterator& it, ConstIterator end,
	    const bool inBrackets, size_t& group_counter, std::vector<TagAction>& pending_actions,
	    std::deque<Node<RegexData, char_t>>& nodes_storage,
	    std::vector<std::unique_ptr<Limits>>& limits_storage, std::uint32_t& class_counter) {
		SubTree<Node<RegexData, char_t>> answer = {{}, {}};
		std::vector<SubTree<Node<RegexData, char_t>>> nodeLayers = {{parents, parents}};
		// Save initial pending actions to restore on alternation
		std::vector<TagAction> initial_actions = pending_actions;
		for (; it != end; it++) {
			if (*it == ')' && inBrackets) {
				break;
			}
			if (*it == '[') {  // start of a set
				const auto latest_parents = nodeLayers.back();
				SubTree<Node<RegexData, char_t>> newNodes =
				    processSet(latest_parents.get_leafs(), regex, it, nodes_storage, class_counter);
				for (auto parent : latest_parents.get_leafs()) {
					for (auto newNode : newNodes.get_leafs()) {
						parent->connect_with(newNode, regex, pending_actions, limits_storage);
					}
				}
				pending_actions.clear();
				nodeLayers.push_back(newNodes);
			} else if (*it == '(') {  // start of a regex in brackets (capture group)
				size_t current_group_id = group_counter++;
				pending_actions.push_back(TagAction::open(current_group_id));  // OPEN_GROUP tag
				it++;
				SubTree<Node<RegexData, char_t>> newLayer =
				    process(nodeLayers.back().get_leafs(), regex, it, end, true, group_counter, pending_actions,
				            nodes_storage, limits_storage,
				            class_counter);                                     // leaves it at the closing bracket
				pending_actions.push_back(TagAction::close(current_group_id));  // CLOSE_GROUP tag
				nodeLayers.push_back(newLayer);
			} else if (*it == '|') {
				answer.roots.insert(answer.roots.end(), nodeLayers[1].get_leafs().begin(),
				                    nodeLayers[1].get_leafs().end());
				answer.leafs.insert(answer.leafs.end(), nodeLayers.back().get_leafs().begin(),
				                    nodeLayers.back().get_leafs().end());
				nodeLayers.resize(1);
				// Restore initial actions for the next alternative branch
				pending_actions = initial_actions;
			} else if (*it == '{') {
				[[maybe_unused]] Limits* limits =
				    processLimit(nodeLayers[nodeLayers.size() - 2], nodeLayers.back(), regex, it, limits_storage);
			} else if (auto special_regex = Node<RegexData, char_t>::special_symbols.find(*it);
			           special_regex != Node<RegexData, char_t>::special_symbols.end()) {
				auto tmp_it = special_regex->second.cbegin();
				[[maybe_unused]] Limits* limits =
				    processLimit(nodeLayers[nodeLayers.size() - 2], nodeLayers.back(), regex, tmp_it, limits_storage);
			} else {  // normal character
				symbol<char_t> sym;
				if (*it == '\\') {  // skip escape symbol
					it++;
					sym = symbol<char_t>(*it);
				} else if (*it == '.') {
					sym = symbol<char_t>::Any;
				} else {
					sym = symbol<char_t>(*it);
				}
				Node<RegexData, char_t>* nextNode = nullptr;
				for (auto parent : nodeLayers.back().get_leafs()) {
					if (parent->neighbours.find(sym) != parent->neighbours.end()) {
						nextNode = parent->neighbours[sym].to;
						break;
					}
				}
				if (nextNode == nullptr) {
					nodes_storage.emplace_back(sym);
					nextNode = &nodes_storage.back();
				}
				for (auto parent : nodeLayers.back().get_leafs()) {
					parent->connect_with(nextNode, regex, pending_actions, limits_storage);
				}
				pending_actions.clear();
				nodeLayers.push_back({{nextNode}, {nextNode}});
			}
		}
		answer.roots.insert(answer.roots.end(), nodeLayers[1].get_leafs().begin(), nodeLayers[1].get_leafs().end());
		answer.leafs.insert(answer.leafs.end(), nodeLayers.back().get_leafs().begin(),
		                    nodeLayers.back().get_leafs().end());
		if (it == end) {
			nodes_storage.emplace_back(symbol<char_t>::EOR);
			Node<RegexData, char_t>* end_of_regex = &nodes_storage.back();
			SubTree<Node<RegexData, char_t>> final_answer = {answer.get_roots(), {end_of_regex}};
			for (auto parent : answer.leafs) {
				parent->connect_with(end_of_regex, regex, pending_actions, limits_storage);
			}
			return final_answer;
		}

		return answer;
	}

	template <typename RegexData, typename char_t>
	template <typename Iterable>
	void RegexMatcher<RegexData, char_t>::add_regex(Iterable str, RegexData uid) {
		auto it = std::cbegin(str);
		size_t group_counter = 0;
		std::vector<TagAction> pending_actions;
		process(std::vector{&root}, uid, it, std::cend(str), false, group_counter, pending_actions, nodes_storage,
		        limits_storage, class_counter);
		// Whatever compile() worked out no longer describes this graph. Cleared rather
		// than updated: a partially stale fast path would silently drop matches, and
		// clearing costs one flag per node against a build that is already the
		// expensive half of this class.
		root.compiled = false;
		for (auto& node : nodes_storage) node.compiled = false;
	}

	template <typename RegexData, typename char_t>
	void RegexMatcher<RegexData, char_t>::compile() {
		if (root.compiled) {
			return;
		}

		// Pass one: per edge, the sorted key run and whether anything on it carries a
		// repeat limit. std::map iterates in key order, so `ids` comes out sorted for
		// free.
		auto describe_edges = [](Node<RegexData, char_t>& node) {
			// Before anything takes a pointer into it. Building doubled its capacity as
			// it went, and trimming moves every edge.
			node.neighbours.shrink();
			for (auto& [sym, edge] : node.neighbours) {
				edge.ids.clear();
				edge.ids.reserve(edge.paths.size());
				edge.has_limits = false;
				for (const auto& [id, limit] : edge.paths) {
					edge.ids.push_back(id);
					if (limit.has_value()) {
						edge.has_limits = true;
					}
				}
			}
		};
		describe_edges(root);
		for (auto& node : nodes_storage) {
			describe_edges(node);
		}

		// Pass two: does taking an edge hand over exactly what is alive at the far end?
		//
		// Alive at a node means the union of its outgoing edges' keys, the end-of-regex
		// edge included, since a regex ending there is still alive there.
		//
		// The unions are cached for the length of this call and then dropped. Cached
		// because a node with many edges pointing into it would otherwise have its
		// union rebuilt once per incoming edge, which made compiling a thousand-route
		// table five times slower than building it. Dropped because along a shared
		// prefix that union is the whole route table, and keeping one per node
		// permanently would cost more memory than the lookup speed is worth.
		std::map<const Node<RegexData, char_t>*, std::vector<RegexData>> live_cache;

		auto live_at = [&live_cache](const Node<RegexData, char_t>* node) -> const std::vector<RegexData>& {
			auto it = live_cache.find(node);
			if (it != live_cache.end()) {
				return it->second;
			}
			std::vector<RegexData> live;
			for (const auto& [child_sym, child_edge] : node->neighbours) {
				live.insert(live.end(), child_edge.ids.begin(), child_edge.ids.end());
			}
			std::sort(live.begin(), live.end());
			live.erase(std::unique(live.begin(), live.end()), live.end());
			return live_cache.emplace(node, std::move(live)).first->second;
		};

		// Pass two and a half: collapse each edge's literal tail into a run.
		//
		// A pattern's literal stretches are chains of states with one way out, and the
		// walk spent one lookup and one call on each character of them. Following the
		// chain here instead means the walk compares the whole stretch in one pass, the
		// way a radix tree compares a stored segment.
		//
		// A state joins the run only if nothing observable happens at it: exactly one way
		// out, that way is a plain character, it carries no capture and no repeat limit,
		// no pattern ends there, and the same set of patterns is alive through it. The
		// last condition is what makes this safe for the set arithmetic above: if the
		// live set cannot change along the run, skipping the intermediate states cannot
		// change what the walk would have concluded.
		auto build_run = [](EdgeInfo<RegexData, Node<RegexData, char_t>, char_t>& edge) {
			edge.run.clear();
			edge.run_to = nullptr;

			Node<RegexData, char_t>* at = edge.to;
			while (at != nullptr && at->neighbours.size() == 1) {
				const auto& [next_sym, next_edge] = *at->neighbours.begin();
				if (next_sym.is_class() || next_sym == symbol<char_t>::Any || next_sym == symbol<char_t>::None ||
				    next_sym == symbol<char_t>::EOR) {
					break;
				}
				if (next_edge.has_limits || !next_edge.tag_actions.empty() || next_edge.to == nullptr) {
					break;
				}
				if (next_edge.ids != edge.ids) {
					break;
				}
				edge.run.push_back(next_sym.ch);
				at = next_edge.to;
				edge.run_to = at;
			}
		};

		auto flag_edges = [&live_at, &build_run](Node<RegexData, char_t>& node) {
			for (auto& [sym, edge] : node.neighbours) {
				build_run(edge);
				// Against the far end of the run, which is where the next frame starts.
				Node<RegexData, char_t>* landing = edge.run.empty() ? edge.to : edge.run_to;
				edge.gives_full_child = (landing != nullptr) && (edge.ids == live_at(landing));
			}
			node.has_any_edge = node.neighbours.find(symbol<char_t>::Any) != node.neighbours.end();
			node.has_none_edge = node.neighbours.find(symbol<char_t>::None) != node.neighbours.end();
			// Class edges cannot be found by key, since the key is the class rather than
			// the character being read, so they are gathered here and scanned. Almost no
			// node has one, and none has many.
			if (node.extras) {
				node.extras->edges.clear();
				node.extras->repeats.clear();
			}
			for (const auto& [sym, edge] : node.neighbours) {
				if (sym.is_class()) {
					node.ensure_extras().edges.push_back(&edge);
				}
				// Which repeats leaving here could cut short. Only bounds above zero can:
				// a + or a * is already satisfied by the edge that entered it.
				for (const auto& [id, limit] : edge.paths) {
					if (limit.has_value() && limit.value()->from_quantifier && limit.value()->min > 0) {
						node.ensure_extras().repeats.emplace_back(id, limit.value());
					}
				}
			}
			node.has_repeat_bounds = node.extras && !node.extras->repeats.empty();

			// Give back the capacity building left behind. Nothing is added after this
			// without add_regex, which clears `compiled` and sends everything through
			// here again, so the structure is only ever trimmed once it is final.
			//
			// This is the whole reason the arm loses at ten thousand routes: measured
			// against a query stream small enough to stay in cache it is faster than the
			// radix tree, and slower only once the structure stops fitting.
			// Note what is NOT trimmed here: the neighbour run itself. Shrinking it
			// moves the edges, and the class-edge list gathered just above points into
			// them. That is done in the first pass instead, before anything holds a
			// pointer.
			for (auto& [sym, edge] : node.neighbours) {
				edge.paths.shrink();
				edge.tag_actions.shrink();
				edge.ids.shrink_to_fit();
				edge.run.shrink_to_fit();
			}
			if (node.extras) {
				node.extras->members.shrink_to_fit();
				node.extras->edges.shrink_to_fit();
				node.extras->repeats.shrink_to_fit();
			}
			node.compiled = true;
		};
		flag_edges(root);
		for (auto& node : nodes_storage) {
			flag_edges(node);
		}


	}

	template <typename RegexData, typename char_t>
	template <typename Iterable>
	std::vector<RegexData> RegexMatcher<RegexData, char_t>::match(Iterable str) const {
		return root.match(std::cbegin(str), std::cend(str));
	}

	template <typename RegexData, typename char_t>
	template <typename Iterable>
	std::vector<MatchResult<RegexData>> RegexMatcher<RegexData, char_t>::match_with_groups(Iterable str) const {
		return root.match_with_groups(std::cbegin(str), std::cend(str));
	}

}  // namespace matcher
