#ifndef NODE_IMPL
#define NODE_IMPL

#include <matcher/core.hpp>
#include <stack>

namespace
{
	template <typename RegexData, typename T> std::vector<RegexData> common_values(const std::vector<RegexData>& sorted, const std::map<RegexData, T>& paths)
	{
		std::vector<RegexData> answer;
		if (sorted.empty())
		{
			for (const auto [k, _] : paths)
			{
				answer.push_back(k);
			}
			return answer;
		}
		auto it = paths.cbegin();
		size_t ind = 0;
		while (ind < sorted.size() && it != paths.cend())
		{
			if (it->first == sorted[ind])
			{
				answer.push_back(it->first);
				it++;
				ind++;
			}
			else if (it->first < sorted[ind])
			{
				it++;
			}
			else
			{
				ind++;
			}
		}
		return answer;
	}

	template <typename RegexData, typename char_t> std::list<Limits*> Node<RegexData, char_t>::all_limits = std::list<Limits*>();

	template <typename RegexData, typename char_t>
	std::map<symbol<char_t>, std::string> Node<RegexData, char_t>::special_symbols = {{'+', "{1,}"}, {'*', "{0,}"}, {'?', "{0,1}"}};

	template <typename RegexData, typename char_t> bool Node<RegexData, char_t>::hasChild(symbol<char_t> ch) const
	{
		return (this->neighbours.find(ch) != this->neighbours.end());
	}

	template <typename RegexData, typename char_t> Node<RegexData, char_t>* Node<RegexData, char_t>::getChild(symbol<char_t> ch)
	{
		return this->neighbours.find(ch)->second.to;
	}

	template <typename RegexData, typename char_t> void Node<RegexData, char_t>::absorb(Node<RegexData, char_t>* with)
	{
		if (with == nullptr)
		{
			return;
		}

		for (auto it = with->neighbours.begin(); it != with->neighbours.end();)
		{
			if (!this->hasChild(it->first))
			{
				this->neighbours.insert(with->neighbours.extract(it));
				it = with->neighbours.begin();
			}
			else if (it->second.to)
			{
				auto& current_child = this->neighbours[it->first];
				this->neighbours[it->first].paths.merge(it->second.paths);
				Node* old_child = it->second.to;
				it->second.to = nullptr;
				with->neighbours.erase(it);
				it = with->neighbours.begin();
				this->getChild(current_child.to->current_symbol)->absorb(old_child);
			}
		}

		std::stack<Node<RegexData, char_t>*> st;
		std::set<Node<RegexData, char_t>*> visited;
		st.push(with);
		while (!st.empty())
		{
			Node<RegexData, char_t>* top = st.top();
			st.pop();
			if (visited.find(top) != visited.end())
			{
				continue;
			}
			visited.insert(top);
			for (auto& old_neighbours : top->neighbours)
			{
				if (old_neighbours.second.to == with)
				{
					old_neighbours.second.to = this;
				}
				else
				{
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
	void Node<RegexData, char_t>::connect_with(Node<RegexData, char_t>* child, RegexData regex, std::optional<Limits*> limit)
	{
		if (auto existing_child = neighbours.find(child->current_symbol); existing_child != neighbours.end())
		{
			if (auto it = existing_child->second.paths.find(regex); it != existing_child->second.paths.end())
			{
				if (!it->second.has_value() && limit == std::nullopt)
				{
					auto limit = new Limits(1, 1);
					Node<RegexData, char_t>::all_limits.push_back(limit);
					it->second = limit;
				}
				else if (it->second.has_value() && limit == std::nullopt)
				{
					(it->second.value()->min)++;
					if (it->second.value()->max.has_value())
					{
						(it->second.value()->max.value())++;
					}
				}
			}
			else if (this == child && limit == std::nullopt)
			{
				auto new_limit = new Limits(1, 1);
				Node<RegexData, char_t>::all_limits.push_back(new_limit);
				neighbours[child->current_symbol].paths.emplace(regex, new_limit);
			}
			else
			{
				neighbours[child->current_symbol].paths.emplace(regex, limit);
			}
			return;
		}
		neighbours[child->current_symbol].paths.emplace(regex, limit);
		neighbours[child->current_symbol].to = child;
	}

	template <typename RegexData, typename char_t>
	template <typename ConstIterator>
	std::vector<RegexData> Node<RegexData, char_t>::match(ConstIterator begin, ConstIterator end)
	{
		return match_helper(begin, end, {}, nullptr);
	}

	template <typename RegexData, typename char_t>
	template <typename ConstIterator>
	std::vector<RegexData> Node<RegexData, char_t>::match_helper(ConstIterator begin, ConstIterator end, const std::vector<RegexData>& paths, Node* prev)
	{
		if (begin == end)
		{
			if (auto it = this->neighbours.find(symbol<char_t>::EOR); it != this->neighbours.end())
			{
				std::vector<RegexData> answer;
				std::vector<RegexData> potential_answer = common_values(paths, it->second.paths);
				if (prev != nullptr)
				{
					for (RegexData pathId : potential_answer)
					{
						bool to_include = true;
						if (const auto knot = prev->neighbours.find(this->current_symbol); knot != prev->neighbours.end())
						{
							if (const auto knot_path = it->second.paths.find(pathId); knot_path != it->second.paths.end())
							{
								if (knot_path->second.has_value())
								{
									to_include &= knot_path->second.value()->min == 0;
								}
							}
						}
						if (const auto knot = this->neighbours.find(this->current_symbol); knot != this->neighbours.end())
						{
							if (knot->second.paths.find(pathId) != knot->second.paths.end())
							{
								if (knot->second.paths[pathId].has_value())
								{
									to_include &= knot->second.paths[pathId].value()->min == 0;
								}
							}
						}
						if (to_include)
						{
							answer.push_back(pathId);
						}
					}
					return answer;
				}
				else
				{
					return potential_answer;
				}
			}
			return {};
		}
		std::vector<RegexData> answer;
		const symbol<char_t> current = symbol<char_t>(*begin);
		for (symbol<char_t> to_test : {current, symbol<char_t>::Any, symbol<char_t>::None})
		{
			if (auto it = this->neighbours.find(to_test); it != this->neighbours.end())
			{
				size_t ind = 0;
				std::vector<RegexData> new_paths;
				new_paths.reserve(paths.size());
				std::map<RegexData, std::optional<Limits>> current_paths;
				for (const auto [pathId, limits_ptr] : it->second.paths)
				{
					if (limits_ptr.has_value() && !limits_ptr.value()->is_allowed_to_repeat())
					{
						continue;
					}
					if (prev != nullptr)
					{
						if (paths[ind] > pathId)
						{
							continue;
						}
						while (ind < paths.size() && paths[ind] < pathId)
						{
							ind++;
						}
						if (ind == paths.size())
						{
							break;
						}
					}
					if (prev == nullptr || paths[ind] == pathId)
					{
						new_paths.push_back(pathId);
						if (!limits_ptr.has_value())
						{
							continue;
						}
						current_paths.emplace(pathId, *limits_ptr.value());
						--(*limits_ptr.value());
					}
				}
				if (!new_paths.empty())
				{
					if (to_test != symbol<char_t>::None)
					{
						begin++;
					}
					for (auto match : it->second.to->match_helper(begin, end, new_paths, this))
					{
						answer.push_back(match);
					}
					if (to_test != symbol<char_t>::None)
					{
						begin--;
					}
					for (const auto [pathId, old_limits] : current_paths)
					{
						if (old_limits.has_value())
						{
							(*it->second.paths[pathId].value()) = old_limits.value();
						}
					}
				}
			}
		}
		return answer;
	}

#ifdef DEBUG
	template <typename RegexData, typename char_t>
	void Node<RegexData, char_t>::print_helper(
		size_t layer, std::set<const Node<RegexData, char_t>*>& traversed, std::map<const Node<RegexData, char_t>*, std::string>& nodes) const
	{
		if (traversed.find(this) != traversed.end())
		{
			return;
		}
		const std::basic_string<char_t> layer_str = (std::basic_stringstream<char_t>() << layer).str() + "_";
		const std::basic_string<char_t> next_layer = (std::basic_stringstream<char_t>() << (layer + 1)).str() + "_";
		traversed.emplace(this);
		nodes.emplace(this, layer_str + current_symbol.to_string());
		for (auto child : neighbours)
		{
			if (nodes.find(child.second.to) == nodes.end())
			{
				nodes.emplace(child.second.to, next_layer + child.second.to->current_symbol.to_string());
			}
			std::cout << nodes[this] << " " << nodes[child.second.to] << " ";
			std::cout << child.second.paths.begin()->first << Limits::to_string(child.second.paths.begin()->second);
			for (auto it = std::next(child.second.paths.begin()); it != child.second.paths.end(); it++)
			{
				std::cout << "," << it->first << Limits::to_string(it->second);
			}
			std::cout << std::endl;
			if (nodes.find(child.second.to) != nodes.end())
			{
				child.second.to->print_helper(layer + 1, traversed, nodes);
			}
		}
	}

	template <typename RegexData, typename char_t> void Node<RegexData, char_t>::print() const
	{
		std::set<const Node<RegexData, char_t>*> traversed;
		std::map<const Node<RegexData, char_t>*, std::string> nodes;
		print_helper(0, traversed, nodes);
	}
#endif
} // namespace

#endif
