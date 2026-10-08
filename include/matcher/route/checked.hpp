#pragma once

// The checked front end. A route pattern is a template argument and is checked when the
// program is compiled; each parameter has a C++ type (std::string_view, std::uint64_t or
// std::int64_t); a handler is checked against its pattern and called with the captured
// values converted; and a compile-time table can hold route declarations with their
// handlers, where two routes of one method that match the same paths do not compile.
//
//     struct Request;
//     Response get_user(std::uint64_t id, Request& req);
//     using H = matcher::route::handlers<Response, Request&>;
//     inline constexpr H::decl kApi[] = {H::get<"/users/{id:u64}", &get_user>()};
//     inline constexpr auto kTable = matcher::route::make_route_table<kApi>();
//     // a lookup, then: kTable.routes[match.route].handler(match.values(), request)
//
// A handler takes one argument per parameter of its pattern, in order, then the context
// arguments the table names (none, or for example a server's request).

#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include <matcher/route/lookup.hpp>
#include <matcher/route/method.hpp>
#include <matcher/route/pattern.hpp>
#include <matcher/route/static_table.hpp>
#include <matcher/route/table.hpp>

namespace matcher::route
{

	// A string literal as a template argument: checked_pattern<"/users/{id:u64}">.
	template <std::size_t N>
	struct fixed_string
	{
		char data[N]{};

		constexpr fixed_string(const char (&s)[N]) noexcept
		{
			for (std::size_t i = 0; i < N; ++i)
			{
				data[i] = s[i];
			}
		}
		constexpr std::string_view view() const noexcept { return {data, N - 1}; }
	};

	// The parsed pattern. Each malformed form stops the compilation with a message that names it.
	template <fixed_string P>
	struct checked_pattern
	{
		static constexpr Pattern value = parse_pattern(P.view());
		using E = PatternError;
		static_assert(value.error != E::NoLeadingSlash, "route pattern: must start with '/'");
		static_assert(value.error != E::EmptySegment, "route pattern: empty segment");
		static_assert(value.error != E::UnclosedBrace, "route pattern: unclosed '{'");
		static_assert(value.error != E::BraceInLiteral, "route pattern: '{' or '}' inside a literal segment");
		static_assert(value.error != E::EmptyName, "route pattern: empty parameter name");
		static_assert(value.error != E::BadName, "route pattern: parameter name is not [A-Za-z_][A-Za-z0-9_]*");
		static_assert(value.error != E::RepeatedName, "route pattern: repeated parameter name");
		static_assert(value.error != E::UnknownType, "route pattern: unknown parameter type (string, u64, i64)");
		static_assert(value.error != E::RestNotLast, "route pattern: catch-all {*name} is not the last segment");
		static_assert(value.error != E::NotPchar, "route pattern: a literal byte is not an RFC 3986 pchar");
		static_assert(value.error != E::TooManySegments, "route pattern: more than 32 segments");
		static_assert(value.error != E::TooManyParams, "route pattern: more than 16 parameters");
	};

	namespace detail
	{
		template <ParamType T>
		struct param_value;
		template <>
		struct param_value<ParamType::String>
		{
			using type = std::string_view;
		};
		template <>
		struct param_value<ParamType::U64>
		{
			using type = std::uint64_t;
		};
		template <>
		struct param_value<ParamType::I64>
		{
			using type = std::int64_t;
		};
	}  // namespace detail

	// The C++ type of parameter I of pattern P.
	template <fixed_string P, std::size_t I>
	using param_t = typename detail::param_value<checked_pattern<P>::value.param_type(I)>::type;

	namespace detail
	{
		template <class F>
		struct callable_arity : callable_arity<decltype(&std::remove_cvref_t<F>::operator())>
		{
		};
		template <class R, class... A>
		struct callable_arity<R(A...)>
		{
			static constexpr std::size_t value = sizeof...(A);
		};
		template <class R, class... A>
		struct callable_arity<R(A...) noexcept> : callable_arity<R(A...)>
		{
		};
		template <class R, class... A>
		struct callable_arity<R (*)(A...)> : callable_arity<R(A...)>
		{
		};
		template <class R, class... A>
		struct callable_arity<R (*)(A...) noexcept> : callable_arity<R(A...)>
		{
		};
		template <class C, class R, class... A>
		struct callable_arity<R (C::*)(A...)> : callable_arity<R(A...)>
		{
		};
		template <class C, class R, class... A>
		struct callable_arity<R (C::*)(A...) const> : callable_arity<R(A...)>
		{
		};
		template <class C, class R, class... A>
		struct callable_arity<R (C::*)(A...) noexcept> : callable_arity<R(A...)>
		{
		};
		template <class C, class R, class... A>
		struct callable_arity<R (C::*)(A...) const noexcept> : callable_arity<R(A...)>
		{
		};

		// A function, a pointer to one, or a class with one non-template call operator. A
		// generic lambda has no fixed arity; it is checked by invocability alone.
		template <class F>
		concept has_fixed_arity = std::is_function_v<std::remove_pointer_t<std::remove_cvref_t<F>>> ||
		                          requires { &std::remove_cvref_t<F>::operator(); };

		template <class F>
		consteval std::size_t arity_or(std::size_t fallback)
		{
			if constexpr (has_fixed_arity<F>)
			{
				return callable_arity<std::remove_cvref_t<F>>::value;
			}
			else
			{
				return fallback;
			}
		}

		// A captured value as its parameter's type. The lookup matched a typed value against its
		// type, so the conversion cannot fail.
		template <class T>
		T convert(std::string_view s) noexcept
		{
			if constexpr (std::is_same_v<T, std::string_view>)
			{
				return s;
			}
			else
			{
				T v{};
				std::from_chars(s.data(), s.data() + s.size(), v);
				return v;
			}
		}
	}  // namespace detail

	// Whether handler type F fits pattern P with the trailing context arguments Context...: one
	// argument per parameter, then the context. Two static_asserts say what is wrong.
	template <fixed_string P, class F, class... Context>
	struct handler_check
	{
		static constexpr std::size_t params = checked_pattern<P>::value.params;
		static constexpr bool arity_ok = detail::arity_or<F>(params + sizeof...(Context)) == params + sizeof...(Context);
		static_assert(arity_ok,
		              "route handler: takes a different number of parameters than the route pattern has (one per "
		              "parameter, then the context arguments)");

		template <std::size_t... I>
		static constexpr bool fits(std::index_sequence<I...>)
		{
			return std::is_invocable_v<F, param_t<P, I>..., Context...>;
		}
		static_assert(!arity_ok || fits(std::make_index_sequence<params>{}),
		              "route handler: its parameters do not match the route pattern (a string parameter is "
		              "std::string_view, {x:u64} std::uint64_t, {x:i64} std::int64_t, then the context arguments)");

		static constexpr bool value = true;
	};

	template <fixed_string P, class F, class... Context>
	consteval bool check_handler()
	{
		return handler_check<P, F, Context...>::value;
	}

	// Calls F with the captured values of a match of P, converted to their types, then the
	// context arguments.
	template <fixed_string P, auto F, class... Context>
	decltype(auto) invoke(std::span<const Capture> values, Context&&... ctx)
	{
		static_assert(handler_check<P, decltype(F), Context...>::value);
		return [&]<std::size_t... I>(std::index_sequence<I...>) -> decltype(auto) {
			return std::invoke(F, detail::convert<param_t<P, I>>(values[I].view())..., std::forward<Context>(ctx)...);
		}(std::make_index_sequence<checked_pattern<P>::value.params>{});
	}

	// ========================================================================
	// Route declarations with handlers, and compile-time tables of them
	// ========================================================================

	// One route: its method, its pattern, and a function that converts the captured values and
	// calls the handler.
	template <class R, class... Context>
	struct Decl
	{
		unsigned method = 0;
		std::string_view pattern;
		R (*handler)(std::span<const Capture>, Context...) = nullptr;
	};

	namespace detail
	{
		template <fixed_string P, auto F, class R, class... Context>
		struct thunk
		{
			static R call(std::span<const Capture> values, Context... ctx)
			{
				return route::invoke<P, F, Context...>(values, std::forward<Context>(ctx)...);
			}
		};

		template <fixed_string P, class F, class R, class... Context>
		struct returns_check
		{
			template <std::size_t... I>
			static constexpr bool fits(std::index_sequence<I...>)
			{
				return std::is_invocable_r_v<R, F, param_t<P, I>..., Context...>;
			}
			static constexpr bool value = fits(std::make_index_sequence<checked_pattern<P>::value.params>{});
		};
	}  // namespace detail

	// Declarations for handlers that return R and take the context arguments Context...:
	//     using H = handlers<Response, Request&>;
	//     inline constexpr H::decl kApi[] = {H::get<"/users/{id:u64}", &get_user>(), ...};
	template <class R, class... Context>
	struct handlers
	{
		using decl = Decl<R, Context...>;

		template <Method M, fixed_string P, auto F>
		static consteval decl route()
		{
			static_assert(handler_check<P, decltype(F), Context...>::value);
			static_assert(!handler_check<P, decltype(F), Context...>::arity_ok ||
			                  detail::returns_check<P, decltype(F), R, Context...>::value,
			              "route handler: its return type does not convert to the table's");
			return {index(M), P.view(), &detail::thunk<P, F, R, Context...>::call};
		}
		template <fixed_string P, auto F>
		static consteval decl get()
		{
			return route<Method::Get, P, F>();
		}
		template <fixed_string P, auto F>
		static consteval decl post()
		{
			return route<Method::Post, P, F>();
		}
		template <fixed_string P, auto F>
		static consteval decl put()
		{
			return route<Method::Put, P, F>();
		}
		template <fixed_string P, auto F>
		static consteval decl del()
		{
			return route<Method::Delete, P, F>();
		}
		template <fixed_string P, auto F>
		static consteval decl patch()
		{
			return route<Method::Patch, P, F>();
		}
		template <fixed_string P, auto F>
		static consteval decl head()
		{
			return route<Method::Head, P, F>();
		}
		template <fixed_string P, auto F>
		static consteval decl options()
		{
			return route<Method::Options, P, F>();
		}
	};

	// A table built while compiling, with the declarations it was built from: a match's route
	// is the index of its declaration in `routes`.
	template <std::size_t N, class Table, class D>
	struct RouteTable
	{
		Table table;
		std::array<D, N> routes;
	};

	namespace detail
	{
		template <auto& Decls>
		struct route_specs
		{
			static constexpr std::size_t n = std::size(Decls);
			static constexpr std::array<RouteSpec, n> value = [] {
				std::array<RouteSpec, n> s{};
				for (std::size_t i = 0; i < n; ++i)
				{
					s[i] = {Decls[i].method, Decls[i].pattern, static_cast<std::uint32_t>(i)};
				}
				return s;
			}();
		};
	}  // namespace detail

	// Two routes of one method that match the same paths stop the compilation.
	template <auto& Decls>
	consteval auto make_route_table()
	{
		using Specs = detail::route_specs<Decls>;
		using D = std::remove_cvref_t<decltype(Decls[0])>;
		RouteTable<Specs::n, decltype(make_static_table<Specs::value>()), D> t{make_static_table<Specs::value>(), {}};
		for (std::size_t i = 0; i < Specs::n; ++i)
		{
			t.routes[i] = Decls[i];
		}
		return t;
	}

}  // namespace matcher::route
