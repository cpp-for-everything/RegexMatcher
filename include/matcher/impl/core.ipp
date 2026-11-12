/**
 * @file core.ipp
 * @brief Template implementation for RegexMatcher class: the parser and the automaton builder
 * @note This file is included by core.hpp - do not include directly
 */

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace matcher {

	namespace detail {
		// Escapes of letters and digits (\d, \w, \s, \n, \1 ...) are classes or references that this
		// engine does not implement. They are rejected instead of being read as the plain letter.
		template <typename char_t>
		inline bool is_unsupported_escape(char_t c) {
			return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
		}

		// The most states one pattern may add once its repeat counts are written out. A pattern above it
		// is rejected, so a pattern such as (a{1000}){1000} cannot take all memory.
		constexpr size_t max_pattern_states = size_t(1) << 16;

		// Saturating arithmetic for the size estimate: any value above the limit stands for "too large".
		inline size_t capped_add(size_t a, size_t b) {
			return (a > max_pattern_states || b > max_pattern_states) ? max_pattern_states + 1 : a + b;
		}
		inline size_t capped_mul(size_t a, size_t b) {
			if (a == 0 || b == 0) {
				return 0;
			}
			return (a > max_pattern_states / b) ? max_pattern_states + 1 : a * b;
		}

		// The groups inside an item are numbered one after the other: [first, end). Empty when it has none.
		inline std::pair<size_t, size_t> group_range(const Ast& ast) {
			std::pair<size_t, size_t> range{std::numeric_limits<size_t>::max(), 0};
			if (ast.kind == Ast::Kind::Group) {
				range = {ast.value, ast.value + 1};
			}
			for (const auto& child : ast.children) {
				const auto inner = group_range(child);
				if (inner.first < inner.second) {
					range.first = std::min(range.first, inner.first);
					range.second = std::max(range.second, inner.second);
				}
			}
			return range;
		}

		inline bool has_groups(const Ast& ast) {
			const auto range = group_range(ast);
			return range.first < range.second;
		}

		// True when the item can match the empty string
		inline bool nullable(const Ast& ast) {
			switch (ast.kind) {
				case Ast::Kind::Class:
					return false;
				case Ast::Kind::Group:
					return nullable(ast.children[0]);
				case Ast::Kind::Concat:
					return std::all_of(ast.children.begin(), ast.children.end(), nullable);
				case Ast::Kind::Alt:
					return std::any_of(ast.children.begin(), ast.children.end(), nullable);
				case Ast::Kind::Repeat:
					return ast.min == 0 || nullable(ast.children[0]);
			}
			return false;
		}

		// A repeat whose optional passes need the ECMAScript rule that a pass must consume a character.
		// The rule only changes captures, so it is needed only when the repeated part has groups and can
		// match the empty string.
		inline bool checked_repeat(const Ast& ast) {
			return ast.kind == Ast::Kind::Repeat && ast.max != std::optional<size_t>(ast.min) &&
			       has_groups(ast.children[0]) && nullable(ast.children[0]);
		}

		// The most checked repeats nested in each other. Each nesting level takes one bit of a 64 bit mask.
		constexpr size_t max_check_levels = 64;
		inline size_t check_levels(const Ast& ast) {
			size_t deepest = 0;
			for (const auto& child : ast.children) {
				deepest = std::max(deepest, check_levels(child));
			}
			return deepest + (checked_repeat(ast) ? 1 : 0);
		}

		// States that build() adds for `ast`, capped just above max_pattern_states.
		inline size_t state_estimate(const Ast& ast) {
			size_t total = 0;
			switch (ast.kind) {
				case Ast::Kind::Class:
					return 1;
				case Ast::Kind::Group:
					return capped_add(2, state_estimate(ast.children[0]));
				case Ast::Kind::Alt:
					total = 1;
					[[fallthrough]];
				case Ast::Kind::Concat:
					for (const auto& child : ast.children) {
						total = capped_add(total, state_estimate(child));
					}
					return total;
				case Ast::Kind::Repeat: {
					// A copy, its Reset state, and Enter and Check states for an optional pass
					const size_t copy = capped_add(state_estimate(ast.children[0]), has_groups(ast.children[0]) ? 3 : 0);
					if (!ast.max.has_value()) {
						return capped_add(capped_mul(capped_add(ast.min, 1), copy), 1);
					}
					return capped_add(capped_mul(ast.max.value(), copy), ast.max.value() - ast.min);
				}
			}
			return total;
		}

		// A text form of an item that is equal for two items exactly when they parse to the same tree.
		inline void item_key(const Ast& ast, std::string& out) {
			switch (ast.kind) {
				case Ast::Kind::Class:
					out += "c" + std::to_string(ast.value);
					return;
				case Ast::Kind::Group:
					out += "g" + std::to_string(ast.value);
					break;
				case Ast::Kind::Concat:
					out += "&";
					break;
				case Ast::Kind::Alt:
					out += "|";
					break;
				case Ast::Kind::Repeat:
					out += "r" + std::to_string(ast.min) + "," +
					       (ast.max.has_value() ? std::to_string(ast.max.value()) : std::string("inf")) +
					       (ast.lazy ? "?" : "");
					break;
			}
			out += "(";
			for (const auto& child : ast.children) {
				item_key(child, out);
				out += " ";
			}
			out += ")";
		}

		inline void remap_classes(Ast& ast, const std::vector<size_t>& global) {
			if (ast.kind == Ast::Kind::Class) {
				ast.value = global[ast.value];
			}
			for (auto& child : ast.children) {
				remap_classes(child, global);
			}
		}
	}  // namespace detail

	/**
	 * @brief Recursive descent parser from a pattern to an Ast. It does not touch the matcher: the
	 * character classes it reads go to its own list.
	 *
	 */
	template <typename RegexData, typename char_t>
	template <typename ConstIterator>
	struct RegexMatcher<RegexData, char_t>::Parser {
		ConstIterator it;
		ConstIterator end;
		std::vector<detail::CharClass<char_t>> classes;
		size_t groups = 0;

		detail::Ast parse() {
			detail::Ast ast = alternation(false);
			if (it != end) {
				throw std::invalid_argument("Unmatched ')' in pattern.");
			}
			return ast;
		}

		// Alternatives up to the end of the pattern or the ')' of the group, which is left unread.
		detail::Ast alternation(bool in_group) {
			detail::Ast alt;
			alt.kind = detail::Ast::Kind::Alt;
			for (;;) {
				detail::Ast sequence;
				sequence.kind = detail::Ast::Kind::Concat;
				while (it != end && *it != '|' && *it != ')') {
					sequence.children.push_back(item());
				}
				if (sequence.children.empty()) {
					throw std::invalid_argument("Empty pattern or empty alternative.");
				}
				alt.children.push_back(std::move(sequence));
				if (it == end) {
					if (in_group) {
						throw std::invalid_argument("Unclosed '(' in pattern.");
					}
					return alt;
				}
				if (*it == ')') {
					if (!in_group) {
						throw std::invalid_argument("Unmatched ')' in pattern.");
					}
					return alt;
				}
				++it;  // the '|'
			}
		}

		static bool is_repeat(char_t c) { return c == '*' || c == '+' || c == '?' || c == '{'; }

		// An atom and the repeat that follows it, if any.
		detail::Ast item() {
			detail::Ast body = atom();
			if (it == end || !is_repeat(*it)) {
				return body;
			}
			detail::Ast repeat;
			repeat.kind = detail::Ast::Kind::Repeat;
			if (*it == '*') {
				repeat.min = 0;
				++it;
			} else if (*it == '+') {
				repeat.min = 1;
				++it;
			} else if (*it == '?') {
				repeat.min = 0;
				repeat.max = 1;
				++it;
			} else {
				braces(repeat);
			}
			if (it != end && *it == '?') {
				repeat.lazy = true;
				++it;
			}
			if (it != end && is_repeat(*it)) {
				throw std::invalid_argument("Nothing to repeat in pattern (a repeat follows a repeat).");
			}
			repeat.children.push_back(std::move(body));
			return repeat;
		}

		size_t number() {
			if (it == end) {
				throw std::invalid_argument("Unterminated '{' in pattern.");
			}
			if (!(*it >= '0' && *it <= '9')) {
				throw std::invalid_argument("Malformed repeat count in pattern.");
			}
			size_t value = 0;
			while (it != end && *it >= '0' && *it <= '9') {
				// Above the state limit a count can never be built, so it saturates there.
				value = std::min(value * 10 + static_cast<size_t>(*it - '0'), detail::max_pattern_states + 1);
				++it;
			}
			return value;
		}

		// {n}, {n,} or {n,m}
		void braces(detail::Ast& repeat) {
			++it;  // the '{'
			repeat.min = number();
			repeat.max = repeat.min;
			if (it != end && *it == ',') {
				++it;
				repeat.max = std::nullopt;
				if (it != end && *it != '}') {
					repeat.max = number();
				}
			}
			if (it == end) {
				throw std::invalid_argument("Unterminated '{' in pattern.");
			}
			if (*it != '}') {
				throw std::invalid_argument("Malformed repeat count in pattern.");
			}
			++it;
			if (repeat.max.has_value() && repeat.max.value() < repeat.min) {
				throw std::invalid_argument("Repeat range out of order in pattern.");
			}
		}

		detail::Ast character_class(detail::CharClass<char_t> cls) {
			detail::Ast ast;
			ast.kind = detail::Ast::Kind::Class;
			ast.value = classes.size();
			classes.push_back(std::move(cls));
			return ast;
		}

		detail::Ast literal(char_t c) {
			detail::CharClass<char_t> cls;
			cls.ranges.push_back({c, c});
			return character_class(std::move(cls));
		}

		detail::Ast atom() {
			const char_t c = *it;
			if (c == '(') {
				++it;
				detail::Ast group;
				group.kind = detail::Ast::Kind::Group;
				group.value = groups++;
				group.children.push_back(alternation(true));
				++it;  // the ')'
				return group;
			}
			if (is_repeat(c)) {
				throw std::invalid_argument("Nothing to repeat in pattern.");
			}
			if (c == '^' || c == '$') {
				throw std::invalid_argument("Anchors '^' and '$' are not supported. Matching is whole string.");
			}
			if (c == '[') {
				return set();
			}
			++it;
			if (c == '.') {
				detail::CharClass<char_t> any;
				any.any = true;
				return character_class(std::move(any));
			}
			if (c == '\\') {
				if (it == end) {
					throw std::invalid_argument("Trailing backslash in pattern.");
				}
				if (detail::is_unsupported_escape(*it)) {
					throw std::invalid_argument("Unsupported escape in pattern.");
				}
				return literal(*it++);
			}
			return literal(c);
		}

		// One character of a set, escapes read
		char_t set_character() {
			if (*it == '\\') {
				++it;
				if (it == end) {
					throw std::invalid_argument("Trailing backslash in set.");
				}
				if (detail::is_unsupported_escape(*it)) {
					throw std::invalid_argument("Unsupported escape in set.");
				}
			}
			return *it++;
		}

		detail::Ast set() {
			++it;  // the '['
			if (it != end && *it == '^') {
				throw std::invalid_argument("Negated sets '[^...]' are not supported.");
			}
			if (it != end && *it == ']') {
				throw std::invalid_argument("Empty sets '[]' are not supported.");
			}
			detail::CharClass<char_t> cls;
			for (;;) {
				if (it == end) {
					throw std::invalid_argument("Unterminated '[' in pattern.");
				}
				if (*it == ']') {
					++it;
					break;
				}
				const char_t low = set_character();
				if (it == end) {
					throw std::invalid_argument("Unterminated '[' in pattern.");
				}
				auto after = it;
				if (*it != '-' || ++after == end || *after == ']') {  // a '-' before ']' is a plain '-'
					if (after == end) {
						throw std::invalid_argument("Unterminated '[' in pattern.");
					}
					cls.ranges.push_back({low, low});
					continue;
				}
				++it;  // the '-'
				const char_t high = set_character();
				if (high < low) {
					throw std::invalid_argument("Set range out of order in pattern.");
				}
				cls.ranges.push_back({low, high});
			}
			return character_class(std::move(cls));
		}
	};

	template <typename RegexData, typename char_t>
	RegexMatcher<RegexData, char_t>::RegexMatcher() {
		states.emplace_back();  // the start state, a split
		prefixes.emplace_back();
	}

	template <typename RegexData, typename char_t>
	size_t RegexMatcher<RegexData, char_t>::add_class(const detail::CharClass<char_t>& cls) {
		const auto found = std::find(classes.begin(), classes.end(), cls);
		if (found != classes.end()) {
			return static_cast<size_t>(found - classes.begin());
		}
		classes.push_back(cls);
		return classes.size() - 1;
	}

	template <typename RegexData, typename char_t>
	uint32_t RegexMatcher<RegexData, char_t>::new_state(detail::State::Kind kind, uint32_t value) {
		if (states.size() >= std::numeric_limits<uint32_t>::max()) {
			throw std::length_error("Too many automaton states.");
		}
		states.emplace_back();
		states.back().kind = kind;
		states.back().value = value;
		return static_cast<uint32_t>(states.size() - 1);
	}

	// Builds the states of `ast` backwards: they end in `next`, and the entry state is returned.
	template <typename RegexData, typename char_t>
	uint32_t RegexMatcher<RegexData, char_t>::build(const detail::Ast& ast, uint32_t next) {
		using Kind = detail::Ast::Kind;
		using StateKind = detail::State::Kind;
		switch (ast.kind) {
			case Kind::Class: {
				const uint32_t state = new_state(StateKind::Char, static_cast<uint32_t>(ast.value));
				states[state].out = {next};
				return state;
			}
			case Kind::Group: {
				const uint32_t close = new_state(StateKind::Close, static_cast<uint32_t>(ast.value));
				states[close].out = {next};
				const uint32_t body = build(ast.children[0], close);
				const uint32_t open = new_state(StateKind::Open, static_cast<uint32_t>(ast.value));
				states[open].out = {body};
				return open;
			}
			case Kind::Concat:
				for (auto child = ast.children.rbegin(); child != ast.children.rend(); ++child) {
					next = build(*child, next);
				}
				return next;
			case Kind::Alt: {
				if (ast.children.size() == 1) {
					return build(ast.children[0], next);
				}
				std::vector<uint32_t> entries;
				for (const auto& child : ast.children) {
					entries.push_back(build(child, next));
				}
				const uint32_t split = new_state(StateKind::Split);
				states[split].out = std::move(entries);
				return split;
			}
			case Kind::Repeat:
				return build_repeat(ast, next);
		}
		return next;
	}

	// x{n,m} is written out as n copies of x and then m - n nested optional copies, x{n,} as n - 1
	// copies and a loop. So a repeat needs no counter. Each copy of a part with groups starts with a
	// Reset state: like ECMAScript, a new pass of a repeat forgets the captures of the last pass. For a
	// checked repeat (see detail::checked_repeat) each optional pass sits between Enter and Check states.
	template <typename RegexData, typename char_t>
	uint32_t RegexMatcher<RegexData, char_t>::build_repeat(const detail::Ast& ast, uint32_t next) {
		using StateKind = detail::State::Kind;
		const detail::Ast& body = ast.children[0];
		const bool checked = detail::checked_repeat(ast);
		const auto groups = detail::group_range(body);
		const auto copy = [&](uint32_t to) {
			const uint32_t entry = build(body, to);
			if (groups.first >= groups.second) {
				return entry;
			}
			const uint32_t reset = new_state(StateKind::Reset, static_cast<uint32_t>(groups.first));
			states[reset].value_end = static_cast<uint32_t>(groups.second);
			states[reset].out = {entry};
			return reset;
		};
		const auto optional_pass = [&](uint32_t to) {
			if (!checked) {
				return copy(to);
			}
			const uint32_t check = new_state(StateKind::Check, check_level);
			states[check].out = {to};
			check_level++;
			const uint32_t entry = copy(check);
			check_level--;
			const uint32_t enter = new_state(StateKind::Enter, check_level);
			states[enter].out = {entry};
			has_checks = true;
			return enter;
		};
		const auto loop_split = [&](uint32_t split, uint32_t entry) {
			states[split].out = ast.lazy ? std::vector<uint32_t>{next, entry} : std::vector<uint32_t>{entry, next};
		};
		uint32_t current = next;
		size_t mandatory = ast.min;
		if (!ast.max.has_value()) {
			const uint32_t split = new_state(StateKind::Split);
			if (ast.min == 0 || checked) {
				const uint32_t entry = optional_pass(split);
				loop_split(split, entry);
				current = split;
			} else {
				const uint32_t entry = copy(split);
				loop_split(split, entry);
				current = entry;  // x+: one pass, then the split loops back
				mandatory = ast.min - 1;
			}
		} else {
			for (size_t i = ast.min; i < ast.max.value(); i++) {
				const uint32_t entry = optional_pass(current);
				const uint32_t split = new_state(StateKind::Split);
				loop_split(split, entry);
				current = split;
			}
		}
		for (size_t i = 0; i < mandatory; i++) {
			current = copy(current);
		}
		return current;
	}

	// Adds the items of one alternative after the longest prefix of items already in the prefix tree.
	template <typename RegexData, typename char_t>
	void RegexMatcher<RegexData, char_t>::add_sequence(const std::vector<detail::Ast>& items, uint32_t accept) {
		size_t node = 0;
		for (const auto& item : items) {
			std::string key;
			detail::item_key(item, key);
			const auto& children = prefixes[node].children;
			const auto found = std::find_if(children.begin(), children.end(),
			                                [&key](const auto& child) { return child.first == key; });
			if (found != children.end()) {
				node = found->second;
				continue;
			}
			const uint32_t exit = new_state(detail::State::Kind::Split);
			const uint32_t entry = build(item, exit);
			states[prefixes[node].exit].out.push_back(entry);
			const size_t child = prefixes.size();
			prefixes.emplace_back();
			prefixes.back().exit = exit;
			prefixes[node].children.emplace_back(std::move(key), child);
			node = child;
		}
		states[prefixes[node].exit].out.push_back(accept);
	}

	// Sharing rule. Only whole leading items are shared, and only through the prefix tree, whose nodes
	// are entered from one place: the end of the same items. So a shared state is reached by the same
	// texts and carries the same captures for every regex that uses it. Every other state belongs to one
	// regex. A regex with groups and several top level alternatives is added without sharing: its
	// alternatives keep their order, which decides its captures.
	template <typename RegexData, typename char_t>
	template <typename Iterable>
	void RegexMatcher<RegexData, char_t>::add_regex(Iterable str, RegexData uid) {
		// Parse and check everything first, so a rejected pattern leaves the matcher unchanged
		Parser<decltype(std::cbegin(str))> parser{std::cbegin(str), std::cend(str), {}, 0};
		detail::Ast ast = parser.parse();
		if (detail::state_estimate(ast) > detail::max_pattern_states) {
			throw std::invalid_argument("Pattern too large: its repeat counts write out too many states.");
		}
		if (detail::check_levels(ast) > detail::max_check_levels) {
			throw std::invalid_argument("Pattern too deep: too many repeated groups that can match empty, nested.");
		}

		std::vector<size_t> global;
		for (const auto& cls : parser.classes) {
			global.push_back(add_class(cls));
		}
		detail::remap_classes(ast, global);
		max_groups = std::max(max_groups, parser.groups);
		table_cache.reset();

		const uint32_t accept = new_state(detail::State::Kind::Accept, static_cast<uint32_t>(accept_ids.size()));
		accept_ids.push_back(uid);
		if (parser.groups > 0 && ast.children.size() > 1) {
			const uint32_t entry = build(ast, accept);
			states[0].out.push_back(entry);
			return;
		}
		for (const auto& alternative : ast.children) {
			add_sequence(alternative.children, accept);
		}
	}

}  // namespace matcher
