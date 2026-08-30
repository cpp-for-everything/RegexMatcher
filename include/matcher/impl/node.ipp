/**
 * @file node.ipp
 * @brief Template implementation for Node class
 * @note This file is included by core.hpp - do not include directly
 */

#include <stack>

namespace {
	template <typename RegexData, typename PathMap>
	std::vector<RegexData> common_values(const std::vector<RegexData>& sorted, const PathMap& paths) {
		std::vector<RegexData> answer;
		if (sorted.empty()) {
			for (const auto [k, _] : paths) {
				answer.push_back(k);
			}
			return answer;
		}
		auto it = paths.cbegin();
		size_t ind = 0;
		while (ind < sorted.size() && it != paths.cend()) {
			if (it->first == sorted[ind]) {
				answer.push_back(it->first);
				it++;
				ind++;
			} else if (it->first < sorted[ind]) {
				it++;
			} else {
				ind++;
			}
		}
		return answer;
	}

	template <typename RegexData, typename char_t>
	std::map<symbol<char_t>, std::string> Node<RegexData, char_t>::special_symbols = {
	    {'+', "{1,}"}, {'*', "{0,}"}, {'?', "{0,1}"}};

	template <typename RegexData, typename char_t>
	bool Node<RegexData, char_t>::hasChild(symbol<char_t> ch) const {
		return (this->neighbours.find(ch) != this->neighbours.end());
	}

	template <typename RegexData, typename char_t>
	Node<RegexData, char_t>* Node<RegexData, char_t>::getChild(symbol<char_t> ch) {
		return this->neighbours.find(ch)->second.to;
	}

	template <typename RegexData, typename char_t>
	void Node<RegexData, char_t>::absorb(Node<RegexData, char_t>* with) {
		if (with == nullptr) {
			return;
		}

		for (auto it = with->neighbours.begin(); it != with->neighbours.end();) {
			if (!this->hasChild(it->first)) {
				// Moved across and then dropped, where this used to splice a tree node
				// between two maps. Same effect: the edge leaves `with` and arrives here
				// exactly once, and nothing else refers to it.
				this->neighbours.insert_moved(it->first, std::move(it->second));
				with->neighbours.erase(it);
				if (with->neighbours.size() == 0) {
					break;
				} else {
					it = with->neighbours.begin();
				}
			} else if (it->second.to) {
				auto& current_child = this->neighbours[it->first];
				this->neighbours[it->first].paths.merge(it->second.paths);
				Node* old_child = it->second.to;
				it->second.to = nullptr;
				with->neighbours.erase(it);
				if (current_child.to != nullptr) {
					this->getChild(current_child.to->current_symbol)->absorb(old_child);
				}
				if (with->neighbours.size() == 0) {
					break;
				} else {
					it = with->neighbours.begin();
				}
			}
		}

		std::stack<Node<RegexData, char_t>*> st;
		std::set<Node<RegexData, char_t>*> visited;
		st.push(with);
		while (!st.empty()) {
			Node<RegexData, char_t>* top = st.top();
			st.pop();
			if (visited.find(top) != visited.end()) {
				continue;
			}
			visited.insert(top);
			for (auto& old_neighbours : top->neighbours) {
				if (old_neighbours.second.to == with) {
					old_neighbours.second.to = this;
				} else {
					st.push(old_neighbours.second.to);
				}
			}
		}

		// delete with;

		// std::stack<std::pair<Node<RegexData, char_t>*, Node<RegexData, char_t>* const>> st;
		// std::vector<Node<RegexData, char_t>* const> to_delete;
		// st.push({this, with});
		// while (!st.empty())
		//{
		//	auto el = st.top();
		//	st.pop();
		//	// reattach its children to the root and then connect it
		//	for (const auto& incoming_neighbours : el.second->neighbours)
		//	{
		//		if (el.first->hasChild(incoming_neighbours.first))
		//		{
		//			auto it = el.first->neighbours.find(incoming_neighbours.first)->second;
		//			st.push({it.to, incoming_neighbours.second.to});
		//			to_delete.push_back(incoming_neighbours.second.to);
		//			for (const auto& x : incoming_neighbours.second.paths)
		//			{
		//				it.paths.emplace(x);
		//			}
		//		}
		//		else
		//		{
		//			el.first->neighbours.emplace(incoming_neighbours);
		//		}
		//	}
		//	el.second->neighbours.clear();
		// }
		// for (auto x : to_delete)
		//{
		//	delete x;
		// }
	}

	template <typename RegexData, typename char_t>
	void Node<RegexData, char_t>::connect_with(Node<RegexData, char_t>* child, RegexData regex,
	                                           std::vector<std::unique_ptr<Limits>>& limits_storage,
	                                           std::optional<Limits*> limit) {
		connect_with(child, regex, {}, limits_storage, limit);
	}

	template <typename RegexData, typename char_t>
	void Node<RegexData, char_t>::connect_with(Node<RegexData, char_t>* child, RegexData regex,
	                                           const std::vector<TagAction>& actions,
	                                           std::vector<std::unique_ptr<Limits>>& limits_storage,
	                                           std::optional<Limits*> limit) {
		if (auto existing_child = neighbours.find(child->current_symbol); existing_child != neighbours.end()) {
			if (auto it = existing_child->second.paths.find(regex); it != existing_child->second.paths.end()) {
				if (!it->second.has_value() && limit == std::nullopt) {
					limits_storage.push_back(std::make_unique<Limits>(1, 1));
					auto new_limit_ptr = limits_storage.back().get();
					it->second = new_limit_ptr;
				} else if (it->second.has_value() && limit == std::nullopt) {
					(it->second.value()->min)++;
					if (it->second.value()->max.has_value()) {
						(it->second.value()->max.value())++;
					}
				}
			} else if (this == child && limit == std::nullopt) {
				limits_storage.push_back(std::make_unique<Limits>(1, 1));
				auto new_limit_ptr = limits_storage.back().get();
				neighbours[child->current_symbol].paths.emplace(regex, new_limit_ptr);
			} else {
				neighbours[child->current_symbol].paths.emplace(regex, limit);
			}
			// Append tag actions for this regex path
			if (!actions.empty()) {
				auto& existing_actions = neighbours[child->current_symbol].tag_actions[regex];
				existing_actions.insert(existing_actions.end(), actions.begin(), actions.end());
			}
			return;
		}
		neighbours[child->current_symbol].paths.emplace(regex, limit);
		neighbours[child->current_symbol].to = child;
		// Store tag actions for this regex path
		if (!actions.empty()) {
			neighbours[child->current_symbol].tag_actions[regex] = actions;
		}
	}

	template <typename RegexData, typename char_t>
	template <typename ConstIterator>
	std::vector<RegexData> Node<RegexData, char_t>::match(ConstIterator begin, ConstIterator end) const {
		LimitState limit_state;
		// Every regex is alive at the root; see match_with_groups for the reasoning.
		auto answer = match_helper(begin, end, {}, nullptr, limit_state, true);
		// Ordered by regex, not by the order the walk happened to try edges in.
		//
		// It was incidentally ordered before: a class member and a literal with the same
		// character were the same node, so both patterns rode one edge and came out in
		// key order. With a class as one node they are two edges, and the order became
		// whichever was tried first. Callers do read this order -- the router takes the
		// last match as the most specific one -- so it is made explicit rather than left
		// to the shape of the graph.
		std::stable_sort(answer.begin(), answer.end());
		return answer;
	}

	template <typename RegexData, typename char_t>
	template <typename ConstIterator>
	std::vector<RegexData> Node<RegexData, char_t>::match_helper(ConstIterator begin, ConstIterator end,
	                                                             const std::vector<RegexData>& paths, const Node* prev,
	                                                             LimitState& limit_state, bool paths_is_live) const {
		if (begin == end) {
			if (auto it = this->neighbours.find(symbol<char_t>::EOR); it != this->neighbours.end()) {
				std::vector<RegexData> answer;
				std::vector<RegexData> potential_answer = common_values(paths, it->second.paths);
				if (prev != nullptr) {
					for (RegexData pathId : potential_answer) {
						bool to_include = true;
						if (const auto knot = prev->neighbours.find(this->current_symbol);
						    knot != prev->neighbours.end()) {
							if (const auto knot_path = it->second.paths.find(pathId);
							    knot_path != it->second.paths.end()) {
								if (knot_path->second.has_value()) {
									to_include &= limit_state.current(knot_path->second.value()).min == 0;
								}
							}
						}
						if (const auto knot = this->neighbours.find(this->current_symbol);
						    knot != this->neighbours.end()) {
							if (knot->second.paths.find(pathId) != knot->second.paths.end()) {
								if (knot->second.paths.at(pathId).has_value()) {
									to_include &= limit_state.current(knot->second.paths.at(pathId).value()).min == 0;
								}
							}
						}
						if (to_include) {
							answer.push_back(pathId);
						}
					}
					return answer;
				} else {
					return potential_answer;
				}
			}
			return {};
		}
		std::vector<RegexData> answer;
		const symbol<char_t> current = symbol<char_t>(*begin);
		// The character, then the wildcard, then epsilon, then any class edge whose
		// members contain this character. A class is keyed by the class rather than by
		// what it reads, so its edges cannot be found by lookup and are scanned; almost
		// no node has one. The wildcard and epsilon lookups are skipped where compile()
		// established there is nothing to find, which is nearly always.
		const size_t class_count =
		    this->compiled ? (this->extras ? this->extras->edges.size() : 0) : this->neighbours.size();
		for (size_t candidate = 0; candidate < 3 + class_count; ++candidate) {
			const EdgeInfo<RegexData, Node<RegexData, char_t>, char_t>* edge_ptr = nullptr;
			bool consumes = true;

			if (candidate < 3) {
				if (this->compiled) {
					if (candidate == 1 && !this->has_any_edge) continue;
					if (candidate == 2 && !this->has_none_edge) continue;
				}
				const symbol<char_t> to_test = (candidate == 0)   ? current
				                               : (candidate == 1) ? symbol<char_t>::Any
				                                                  : symbol<char_t>::None;
				const auto it = this->neighbours.find(to_test);
				if (it == this->neighbours.end()) {
					continue;
				}
				edge_ptr = &it->second;
				consumes = (to_test != symbol<char_t>::None);
			} else {
				// compile() gathers the class edges; without it they are found the slow
				// way, so an uncompiled matcher still answers the same.
				const size_t which = candidate - 3;
				if (this->compiled) {
					edge_ptr = this->extras->edges[which];
				} else {
					auto it = this->neighbours.begin();
					std::advance(it, static_cast<std::ptrdiff_t>(which));
					if (!it->first.is_class()) {
						continue;
					}
					edge_ptr = &it->second;
				}
				if (edge_ptr->to == nullptr || !edge_ptr->to->accepts(*begin)) {
					continue;
				}
			}

			{
				const auto& edge = *edge_ptr;
				std::vector<std::pair<const Limits*, Limits>> saved_limits;

				// Same shortcut as match_with_groups_helper, for the same reason. See
				// the comment there: when the arriving set is everything alive at this
				// node and nothing on the edge can prune it, the surviving set is the
				// edge's own precomputed run and there is nothing to rebuild.
				const bool fast = this->compiled && paths_is_live && !edge.has_limits && !this->has_repeat_bounds;

				std::vector<RegexData> built;
				if (!fast) {
					size_t ind = 0;
					built.reserve(paths.size());
					for (const auto [pathId, limits_ptr] : edge.paths) {
						if (limits_ptr.has_value() &&
						    !limit_state.current(limits_ptr.value()).is_allowed_to_repeat()) {
							continue;
						}
						if (prev != nullptr) {
							if (paths[ind] > pathId) {
								continue;
							}
							while (ind < paths.size() && paths[ind] < pathId) {
								ind++;
							}
							if (ind == paths.size()) {
								break;
							}
						}
						// A repeat still owing iterations cannot be stepped out of. Free
						// where no repeat here has a lower bound, which is every node the
						// router generates.
						if (!this->repeat_satisfied(pathId, limits_ptr, limit_state)) {
							continue;
						}
						if (prev == nullptr || paths[ind] == pathId) {
							built.push_back(pathId);
							if (!limits_ptr.has_value()) {
								continue;
							}
							saved_limits.emplace_back(limits_ptr.value(),
							                          limit_state.consume(limits_ptr.value()));
						}
					}
				}

				const std::vector<RegexData>& new_paths = fast ? edge.ids : built;
				const bool next_is_live = fast && edge.gives_full_child;

				// See match_with_groups_helper: the literal tail is compared in one pass.
				ConstIterator after_run = begin;
				if (consumes) {
					++after_run;
				}
				bool run_ok = true;
				for (const char_t expected : edge.run) {
					if (after_run == end || *after_run != expected) {
						run_ok = false;
						break;
					}
					++after_run;
				}
				if (!run_ok) {
					continue;
				}

				if (!new_paths.empty()) {
					for (auto match :
					     edge.run_end()->match_helper(after_run, end, new_paths, this, limit_state, next_is_live)) {
						answer.push_back(match);
					}
					for (auto rit = saved_limits.rbegin(); rit != saved_limits.rend(); ++rit) {
						limit_state.restore(rit->first, rit->second);
					}
				}
			}
		}
		return answer;
	}

	template <typename RegexData, typename char_t>
	template <typename ConstIterator>
	std::vector<matcher::MatchResult<RegexData>> Node<RegexData, char_t>::match_with_groups(ConstIterator begin,
	                                                                                        ConstIterator end) const {
		std::vector<matcher::MatchResult<RegexData>> results;
		CaptureTable<RegexData> capture_slots;
		LimitState limit_state;
		// Every regex is alive at the root, which is exactly what paths_is_live claims,
		// so the walk can start on the fast path. (The general path says the same thing
		// a longer way: with prev == nullptr it accepts every id on the edge.)
		match_with_groups_helper(begin, end, 0, {}, nullptr, capture_slots, limit_state, results, true);
		// By regex, for the reason given in match(). Stable, so several results for one
		// regex keep the order the walk found them in.
		std::stable_sort(results.begin(), results.end(),
		                 [](const matcher::MatchResult<RegexData>& a, const matcher::MatchResult<RegexData>& b) {
			                 return a.regex_id < b.regex_id;
		                 });
		return results;
	}

	template <typename RegexData, typename char_t>
	template <typename ConstIterator>
	void Node<RegexData, char_t>::match_with_groups_helper(
	    ConstIterator begin, ConstIterator end, size_t position, const std::vector<RegexData>& paths, const Node* prev,
	    CaptureTable<RegexData>& capture_slots, LimitState& limit_state,
	    std::vector<matcher::MatchResult<RegexData>>& results, bool paths_is_live) const {
#ifdef MATCHER_COUNT
		++g_calls;
#endif
		if (begin == end) {
			// Check for end-of-regex marker
			if (auto it = this->neighbours.find(symbol<char_t>::EOR); it != this->neighbours.end()) {
				std::vector<RegexData> potential_answer = common_values(paths, it->second.paths);
				for (RegexData pathId : potential_answer) {
					bool to_include = true;
					// Check limit constraints (same logic as match_helper)
					if (prev != nullptr) {
						if (const auto knot = prev->neighbours.find(this->current_symbol);
						    knot != prev->neighbours.end()) {
							if (const auto knot_path = it->second.paths.find(pathId);
							    knot_path != it->second.paths.end()) {
								if (knot_path->second.has_value()) {
									to_include &= limit_state.current(knot_path->second.value()).min == 0;
								}
							}
						}
						if (const auto knot = this->neighbours.find(this->current_symbol);
						    knot != this->neighbours.end()) {
							if (knot->second.paths.find(pathId) != knot->second.paths.end()) {
								if (knot->second.paths.at(pathId).has_value()) {
									to_include &= limit_state.current(knot->second.paths.at(pathId).value()).min == 0;
								}
							}
						}
					}
					if (to_include) {
						// Process any tag actions on the EOR edge
						if (auto actions_it = it->second.tag_actions.find(pathId);
						    actions_it != it->second.tag_actions.end()) {
							for (const auto& action : actions_it->second) {
								if (action.is_open()) {
									capture_slots[pathId].open_group(action.group_id, position);
								} else {
									capture_slots[pathId].close_group(action.group_id, position);
								}
							}
						}
						// Create result with captured groups using CaptureSlots::to_map()
						results.emplace_back(pathId, capture_slots[pathId].to_map());
					}
				}
			}
			return;
		}

		const symbol<char_t> current = symbol<char_t>(*begin);
		// The character, then the wildcard, then epsilon, then any class edge whose
		// members contain this character. A class is keyed by the class rather than by
		// what it reads, so its edges cannot be found by lookup and are scanned; almost
		// no node has one. The wildcard and epsilon lookups are skipped where compile()
		// established there is nothing to find, which is nearly always.
		const size_t class_count =
		    this->compiled ? (this->extras ? this->extras->edges.size() : 0) : this->neighbours.size();
		for (size_t candidate = 0; candidate < 3 + class_count; ++candidate) {
			const EdgeInfo<RegexData, Node<RegexData, char_t>, char_t>* edge_ptr = nullptr;
			bool consumes = true;

			if (candidate < 3) {
				if (this->compiled) {
					if (candidate == 1 && !this->has_any_edge) continue;
					if (candidate == 2 && !this->has_none_edge) continue;
				}
				const symbol<char_t> to_test = (candidate == 0)   ? current
				                               : (candidate == 1) ? symbol<char_t>::Any
				                                                  : symbol<char_t>::None;
				const auto it = this->neighbours.find(to_test);
				if (it == this->neighbours.end()) {
					continue;
				}
				edge_ptr = &it->second;
				consumes = (to_test != symbol<char_t>::None);
			} else {
				// compile() gathers the class edges; without it they are found the slow
				// way, so an uncompiled matcher still answers the same.
				const size_t which = candidate - 3;
				if (this->compiled) {
					edge_ptr = this->extras->edges[which];
				} else {
					auto it = this->neighbours.begin();
					std::advance(it, static_cast<std::ptrdiff_t>(which));
					if (!it->first.is_class()) {
						continue;
					}
					edge_ptr = &it->second;
				}
				if (edge_ptr->to == nullptr || !edge_ptr->to->accepts(*begin)) {
					continue;
				}
			}

			{
				const auto& edge = *edge_ptr;
				std::vector<std::pair<const Limits*, Limits>> saved_limits;

				// The set surviving this edge is the arriving set intersected with the
				// edge's own. When the arriving set is everything alive at this node,
				// that intersection is the edge's set unchanged, because every path on
				// an outgoing edge is by construction alive at the node it leaves. So
				// there is nothing to compute: hand the precomputed run down as it is.
				//
				// Only repeat limits can take paths off an edge, so an edge carrying
				// none cannot narrow the set either. compile() records both facts.
				//
				// This is what stops a lookup costing one pass over the route table per
				// character. Along a shared prefix the arriving set is the whole table,
				// and rebuilding it at every character was the entire cost.
				const bool fast = this->compiled && paths_is_live && !edge.has_limits && !this->has_repeat_bounds;

				std::vector<RegexData> built;
				if (!fast) {
					size_t ind = 0;
					built.reserve(paths.size());
					for (const auto& [pathId, limits_ptr] : edge.paths) {
						if (limits_ptr.has_value() &&
						    !limit_state.current(limits_ptr.value()).is_allowed_to_repeat()) {
							continue;
						}
						if (prev != nullptr) {
							if (paths[ind] > pathId) {
								continue;
							}
							while (ind < paths.size() && paths[ind] < pathId) {
								ind++;
							}
							if (ind == paths.size()) {
								break;
							}
						}
						// A repeat still owing iterations cannot be stepped out of. Free
						// where no repeat here has a lower bound, which is every node the
						// router generates.
						if (!this->repeat_satisfied(pathId, limits_ptr, limit_state)) {
							continue;
						}
						if (prev == nullptr || paths[ind] == pathId) {
							built.push_back(pathId);
							if (!limits_ptr.has_value()) {
								continue;
							}
							saved_limits.emplace_back(limits_ptr.value(),
							                          limit_state.consume(limits_ptr.value()));
						}
					}
				}

				const std::vector<RegexData>& new_paths = fast ? edge.ids : built;
				// Whether the next frame may use the fast path in turn. Only when this
				// edge hands over exactly what is alive there; anything narrower and
				// the next node's own set is no longer the right answer.
				const bool next_is_live = fast && edge.gives_full_child;

				// The collapsed literal tail, matched in one pass. Checked before any
				// capture is opened, so a run that does not match simply moves on to the
				// next candidate with nothing to undo.
				ConstIterator after_run = begin;
				size_t run_advance = 0;
				if (consumes) {
					++after_run;
					run_advance = 1;
				}
				bool run_ok = true;
				for (const char_t expected : edge.run) {
					if (after_run == end || *after_run != expected) {
						run_ok = false;
						break;
					}
					++after_run;
					++run_advance;
				}
				if (!run_ok) {
					continue;
				}

				if (!new_paths.empty()) {
					// Track undo operations for efficient backtracking (avoid full map copy)
					// Each entry: (pathId, group_id, is_open, prev_value)
					std::vector<std::tuple<RegexData, size_t, bool, size_t>> undo_stack;

					// Process tag actions for this edge transition
					// Tag actions are executed BEFORE consuming the current character
					//
					// Guarded, because most edges carry none. Every character of a
					// literal prefix used to cost one map lookup per live regex looking
					// for actions that were never there: at a thousand routes sharing
					// /api/v1/ that was nine thousand lookups per match, the same count
					// as the path walk itself.
					if (!edge.tag_actions.empty()) {
						for (RegexData pathId : new_paths) {
							if (auto actions_it = edge.tag_actions.find(pathId);
							    actions_it != edge.tag_actions.end()) {
								for (const auto& action : actions_it->second) {
									if (action.is_open()) {
										size_t prev = capture_slots[pathId].open_group(action.group_id, position);
										undo_stack.emplace_back(pathId, action.group_id, true, prev);
									} else {
										size_t prev = capture_slots[pathId].close_group(action.group_id, position);
										undo_stack.emplace_back(pathId, action.group_id, false, prev);
									}
								}
							}
						}
					}

					edge.run_end()->match_with_groups_helper(after_run, end, position + run_advance, new_paths, this,
					                                          capture_slots, limit_state, results, next_is_live);

					// Undo capture slot changes (reverse order)
					for (auto rit = undo_stack.rbegin(); rit != undo_stack.rend(); ++rit) {
						const auto& [pathId, group_id, is_open, prev_value] = *rit;
						if (is_open) {
							capture_slots[pathId].undo_open(group_id, prev_value);
						} else {
							capture_slots[pathId].undo_close(group_id, prev_value);
						}
					}

					// Restore this frame's repeat counters (newest first, like the undo stack above)
					for (auto rit = saved_limits.rbegin(); rit != saved_limits.rend(); ++rit) {
						limit_state.restore(rit->first, rit->second);
					}
				}
			}
		}
	}

#ifdef DEBUG
	template <typename RegexData, typename char_t>
	void Node<RegexData, char_t>::print_helper(size_t layer, std::set<const Node<RegexData, char_t>*>& traversed,
	                                           std::map<const Node<RegexData, char_t>*, std::string>& nodes) const {
		if (traversed.find(this) != traversed.end()) {
			return;
		}
		const std::basic_string<char_t> layer_str = (std::basic_stringstream<char_t>() << layer).str() + "_";
		const std::basic_string<char_t> next_layer = (std::basic_stringstream<char_t>() << (layer + 1)).str() + "_";
		traversed.emplace(this);
		nodes.emplace(this, layer_str + current_symbol.to_string());
		for (auto child : neighbours) {
			if (nodes.find(child.second.to) == nodes.end()) {
				nodes.emplace(child.second.to, next_layer + child.second.to->current_symbol.to_string());
			}
			std::cout << nodes[this] << " " << nodes[child.second.to] << " ";
			std::cout << child.second.paths.begin()->first << Limits::to_string(child.second.paths.begin()->second);
			for (auto it = std::next(child.second.paths.begin()); it != child.second.paths.end(); it++) {
				std::cout << "," << it->first << Limits::to_string(it->second);
			}
			std::cout << std::endl;
			if (nodes.find(child.second.to) != nodes.end()) {
				child.second.to->print_helper(layer + 1, traversed, nodes);
			}
		}
	}

	template <typename RegexData, typename char_t>
	void Node<RegexData, char_t>::print() const {
		std::set<const Node<RegexData, char_t>*> traversed;
		std::map<const Node<RegexData, char_t>*, std::string> nodes;
		print_helper(0, traversed, nodes);
	}
#endif
}  // anonymous namespace
