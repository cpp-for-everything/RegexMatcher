#pragma once

#include "regex_matcher.hpp"
#include "range.hpp"

namespace matcher::regex
{
	/**
	 * @brief Attributes of the characters managed by the current node
	 *
	 */
	enum class Attributes
	{
		Unset,
		Start_of_regex,
		PositiveCharacterSet,
		NegativeCharacterSet,
		Wildcard,
		End_of_regex
	};

	/**
	 * @brief Wrapper for the content of the current node in the graph of regexes
	 *
	 * @tparam symbol_t Symbol type
	 */
	template <typename symbol_t> struct NodeInfo
	{
	private:
		Range<symbol_t> charset;
		Attributes attributes;

	public:
		/**
		 * @brief Construct a new NodeInfo object from a single unset symbol
		 *
		 */
		NodeInfo();

		/**
		 * @brief Construct a new NodeInfo object from a single symbol
		 *
		 * @param symbol Positive charset containing only the given symbol
		 */
		explicit NodeInfo(symbol_t symbol);

		/**
		 * @brief Construct a new NodeInfo object from special node
		 *
		 * @param attributes Special node info
		 */
		explicit NodeInfo(Attributes attributes);

		// Copy constructor
		NodeInfo(const NodeInfo<symbol_t>& other);
		NodeInfo& operator=(const NodeInfo<symbol_t>& other);

		// Move constructor
		NodeInfo(NodeInfo<symbol_t>&& other) noexcept;
		NodeInfo& operator=(NodeInfo<symbol_t>&& other) noexcept;

		/**
		 * @brief Comparison operator
		 *
		 */
		auto operator<=>(const NodeInfo<symbol_t>& other) const;
	};
} // namespace matcher::regex
