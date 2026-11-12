/**
 * @file node.ipp
 * @brief Template implementation for RegexMatcher class: the matching engine
 * @note This file is included by core.hpp - do not include directly
 */

#include <algorithm>

namespace matcher {

	// Steps the set of live states one character at a time (a Pike machine). The live states are kept in
	// priority order: the order in which a backtracking engine would try them. A state enters a step's
	// list once, from the first (highest priority) path that reaches it, so each step does work bounded
	// by the size of the automaton, and a long text cannot make the search explode or recurse. Only
	// match_with_groups() on a pattern with Check states may visit an empty move state a few times in
	// one step (see Seen below).
	//
	// The working memory is a scratch buffer lent from the matcher's pool (see detail::ScratchPool). It
	// is resized to this automaton at the start of the call and keeps its capacity, so a call allocates
	// only while the buffers grow.
	template <typename RegexData, typename char_t>
	template <bool WithGroups, typename ConstIterator, typename Sink>
	void RegexMatcher<RegexData, char_t>::run(detail::RunScratch<RegexData>& scratch, ConstIterator begin,
	                                          ConstIterator end, Sink&& sink) const {
		using Kind = detail::State::Kind;
		using Action = detail::RunAction;
		using Item = detail::RunItem;
		using Seen = detail::RunSeen;
		const size_t width = WithGroups ? 2 * max_groups : 0;  // capture slots of one live state

		std::vector<uint64_t>& mark = scratch.mark;
		if (mark.size() < states.size()) {
			mark.resize(states.size(), 0);
		}
		std::vector<uint32_t>& current = scratch.current;
		std::vector<uint32_t>& next = scratch.next;
		current.clear();
		next.clear();
		std::vector<size_t>& current_slots = scratch.current_slots;  // `width` slots per entry of current
		std::vector<size_t>& next_slots = scratch.next_slots;
		current_slots.clear();
		next_slots.clear();
		std::vector<size_t>& work = scratch.work;  // the slots of the path being followed
		work.assign(width, detail::unset);
		std::vector<Item>& stack = scratch.stack;
		stack.clear();

		// With Check states, a path that started a pass at this step may not leave it, while another path
		// to the same state may. So for captures an empty move state is visited again in one step unless
		// an earlier visit had a subset of its `open` bits and has followed all its moves, or had the same
		// bits (a loop back to it that changed nothing). A later visit inside an earlier one that is not
		// finished comes from a new pass of a loop, which a backtracking engine tries before the rest of
		// the earlier visit, so it must not be dropped.
		const bool masked = WithGroups && has_checks;
		std::vector<std::vector<Seen>>& seen = scratch.seen;
		if (masked && seen.size() < states.size()) {
			seen.resize(states.size());
		}

		// Follows the empty moves from `from` and adds the states that consume a character or accept to
		// `list`, in priority order, each with the captures of the path that reached it first.
		const auto follow = [&](std::vector<uint32_t>& list, std::vector<size_t>& slots, uint32_t from,
		                        size_t step, uint64_t stamp) {
			stack.push_back({from, Action::visit, 0, 0, 0});
			while (!stack.empty()) {
				const Item item = stack.back();
				stack.pop_back();
				if (item.action == Action::restore) {
					work[item.slot] = item.value;
					continue;
				}
				if (item.action == Action::finish) {
					seen[item.state][item.slot].finished = true;
					continue;
				}
				const detail::State& state = states[item.state];
				const bool consuming = state.kind == Kind::Char || state.kind == Kind::Accept;
				if (mark[item.state] != stamp) {
					mark[item.state] = stamp;
					if (masked) {
						seen[item.state].clear();
					}
				} else {
					if (!masked || consuming) {
						continue;
					}
					const auto& earlier = seen[item.state];
					if (std::any_of(earlier.begin(), earlier.end(), [&item](const Seen& e) {
						    return (e.open & ~item.open) == 0 && (e.finished || e.open == item.open);
					    })) {
						continue;
					}
				}
				if (masked && !consuming) {
					seen[item.state].push_back({item.open, false});
					stack.push_back({item.state, Action::finish, seen[item.state].size() - 1, 0, 0});
				}
				switch (state.kind) {
					case Kind::Char:
					case Kind::Accept:
						list.push_back(item.state);
						if constexpr (WithGroups) {
							slots.insert(slots.end(), work.begin(), work.end());
						}
						break;
					case Kind::Split:
						for (auto out = state.out.rbegin(); out != state.out.rend(); ++out) {
							stack.push_back({*out, Action::visit, 0, 0, item.open});
						}
						break;
					case Kind::Open:
					case Kind::Close:
						if constexpr (WithGroups) {
							const size_t slot = 2 * size_t(state.value) + (state.kind == Kind::Close ? 1 : 0);
							stack.push_back({0, Action::restore, slot, work[slot], 0});
							work[slot] = step;
						}
						stack.push_back({state.out[0], Action::visit, 0, 0, item.open});
						break;
					case Kind::Reset:
						if constexpr (WithGroups) {
							for (size_t slot = 2 * size_t(state.value); slot < 2 * size_t(state.value_end); slot++) {
								stack.push_back({0, Action::restore, slot, work[slot], 0});
								work[slot] = detail::unset;
							}
						}
						stack.push_back({state.out[0], Action::visit, 0, 0, item.open});
						break;
					case Kind::Enter:
						stack.push_back({state.out[0], Action::visit, 0, 0,
						                 WithGroups ? item.open | (uint64_t(1) << state.value) : 0});
						break;
					case Kind::Check:
						// ECMAScript: an optional pass that consumed nothing fails. Only captures can tell.
						if (WithGroups && (item.open & (uint64_t(1) << state.value)) != 0) {
							break;
						}
						stack.push_back({state.out[0], Action::visit, 0, 0, item.open});
						break;
				}
			}
		};

		size_t step = 0;  // characters consumed so far
		follow(current, current_slots, 0, step, ++scratch.stamp);
		for (auto it = begin; it != end; ++it) {
			if (current.empty()) {
				return;
			}
			const char_t c = *it;
			step++;
			const uint64_t stamp = ++scratch.stamp;
			next.clear();
			next_slots.clear();
			for (size_t i = 0; i < current.size(); i++) {
				const detail::State& state = states[current[i]];
				if (state.kind != Kind::Char || !classes[state.value].contains(c)) {
					continue;
				}
				if constexpr (WithGroups) {
					std::copy(current_slots.begin() + i * width, current_slots.begin() + (i + 1) * width, work.begin());
				}
				follow(next, next_slots, state.out[0], step, stamp);
			}
			current.swap(next);
			current_slots.swap(next_slots);
		}
		for (size_t i = 0; i < current.size(); i++) {
			const detail::State& state = states[current[i]];
			if (state.kind == Kind::Accept) {
				sink(accept_ids[state.value], WithGroups ? current_slots.data() + i * width : nullptr);
			}
		}
	}

#ifdef DEBUG
	template <typename RegexData, typename char_t>
	void RegexMatcher<RegexData, char_t>::print() const {
		static const char* const names[] = {"char", "split", "open", "close", "reset", "accept", "enter", "check"};
		for (size_t s = 0; s < states.size(); s++) {
			std::cout << s << " " << names[static_cast<size_t>(states[s].kind)] << " " << states[s].value << " ->";
			for (const auto out : states[s].out) {
				std::cout << " " << out;
			}
			std::cout << std::endl;
		}
	}
#endif
}  // namespace matcher
