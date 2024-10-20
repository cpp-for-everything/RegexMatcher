#include "node_info.hpp"
#include <utility>

namespace matcher::regex
{
	template <typename symbol_t> NodeInfo<symbol_t>::NodeInfo() : NodeInfo(Attributes::Unset)
	{
	}

	template <typename symbol_t> NodeInfo<symbol_t>::NodeInfo(symbol_t symbol) : charset(symbol), attributes(Attributes::PositiveCharacterSet)
	{
	}

	template <typename symbol_t> NodeInfo<symbol_t>::NodeInfo(Attributes attributes) : charset(), attributes(attributes)
	{
	}

	template <typename symbol_t> NodeInfo<symbol_t>::NodeInfo(const NodeInfo<symbol_t>& other) : charset(other.charset), attributes(other.attributes)
	{
	}

	template <typename symbol_t> NodeInfo<symbol_t>& NodeInfo<symbol_t>::operator=(const NodeInfo<symbol_t>& other)
	{
		charset = other.charset;
		attributes = other.attributes;
		return *this;
	}

	template <typename symbol_t>
	NodeInfo<symbol_t>::NodeInfo(NodeInfo<symbol_t>&& other) noexcept : charset(std::move(other.charset)), attributes(std::move(other.attributes))
	{
	}

	template <typename symbol_t> NodeInfo<symbol_t>& NodeInfo<symbol_t>::operator=(NodeInfo<symbol_t>&& other) noexcept
	{
		charset = std::move(other.charset);
		attributes = std::move(other.attributes);
		return *this;
	}

	template <typename symbol_t> auto NodeInfo<symbol_t>::operator<=>(const NodeInfo<symbol_t>& other) const
	{
		return (other.attributes == attributes) ? (charset <=> other.charset) : (other.attributes <=> attributes);
	}
} // namespace matcher::regex
