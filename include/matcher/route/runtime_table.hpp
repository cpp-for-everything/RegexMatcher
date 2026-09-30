#pragma once

// A table built at run time in one block: the table build_table builds, with its four arrays
// where a compile-time table of the same routes has them (table_layout), in a block aligned to a
// page (kPageAlign). A StaticTable declared alignas(kPageAlign) and the RuntimeTable of the same
// routes then have every element at the same offset into a page, so that neither table's
// placement favours its lookups.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <span>
#include <string_view>
#include <utility>

#include <matcher/route/table.hpp>

namespace matcher::route
{

	class RuntimeTable
	{
	public:
		// As Built's: None, or why the routes did not build a table (and then there is no block).
		BuildError error = BuildError::None;
		std::uint32_t error_route = 0;
		std::uint32_t error_other = 0;

		RuntimeTable() noexcept = default;

		// Copies a built table into one block.
		explicit RuntimeTable(const Built& b) : error(b.error), error_route(b.error_route), error_other(b.error_other)
		{
			if (b.error != BuildError::None)
			{
				return;
			}
			const TableLayout l = table_layout({b.nodes.size(), b.edges.size(), b.literals.size(), b.arena.size()});
			block_ = static_cast<std::byte*>(::operator new(l.bytes, std::align_val_t{kPageAlign}));
			bytes_ = l.bytes;
			const std::span<const Node> nodes = place<Node>(l.nodes, b.nodes);
			const std::span<const Edge> edges = place<Edge>(l.edges, b.edges);
			const std::span<const LiteralSlot> literals = place<LiteralSlot>(l.literals, b.literals);
			char* arena = reinterpret_cast<char*>(block_ + l.arena);
			std::uninitialized_copy(b.arena.begin(), b.arena.end(), arena);
			arena[b.arena.size()] = '\0';
			view_ = {nodes, edges, literals, std::string_view(arena, b.arena.size()), b.roots, b.methods,
			         b.literal_methods};
		}

		RuntimeTable(RuntimeTable&& o) noexcept
			: error(o.error), error_route(o.error_route), error_other(o.error_other),
			  block_(std::exchange(o.block_, nullptr)), bytes_(std::exchange(o.bytes_, 0)),
			  view_(std::exchange(o.view_, TableView{}))
		{
		}

		RuntimeTable& operator=(RuntimeTable&& o) noexcept
		{
			if (this != &o)
			{
				release();
				error = o.error;
				error_route = o.error_route;
				error_other = o.error_other;
				block_ = std::exchange(o.block_, nullptr);
				bytes_ = std::exchange(o.bytes_, 0);
				view_ = std::exchange(o.view_, TableView{});
			}
			return *this;
		}

		RuntimeTable(const RuntimeTable&) = delete;
		RuntimeTable& operator=(const RuntimeTable&) = delete;

		~RuntimeTable()
		{
			release();
		}

		TableView view() const noexcept
		{
			return view_;
		}

		// The block (null when the build failed, or after a move) and its size.
		const std::byte* data() const noexcept
		{
			return block_;
		}
		std::size_t bytes() const noexcept
		{
			return bytes_;
		}

	private:
		// Copies an array into the block at offset `at`; an empty array gets an empty span.
		template <class T, class V>
		std::span<const T> place(std::size_t at, const V& from)
		{
			if (from.empty())
			{
				return {};
			}
			std::uninitialized_copy(from.begin(), from.end(), reinterpret_cast<T*>(block_ + at));
			return {std::launder(reinterpret_cast<T*>(block_ + at)), from.size()};
		}

		void release() noexcept
		{
			if (block_ != nullptr)
			{
				::operator delete(block_, std::align_val_t{kPageAlign});  // unsized: portable without -fsized-deallocation
			}
			block_ = nullptr;
			bytes_ = 0;
			view_ = TableView{};
		}

		std::byte* block_ = nullptr;
		std::size_t bytes_ = 0;
		TableView view_{};
	};

	// Builds a table (build_table) and copies it into one block.
	inline RuntimeTable make_runtime_table(std::span<const RouteSpec> specs)
	{
		return RuntimeTable(build_table(specs));
	}

}  // namespace matcher::route
