#pragma once

// A pointer to member function taken apart, once for every reader: the handler forms read the
// sender off a call operator (events.h), method() spells its call over a member's parameters
// (method.h), an iteration argument names its steps and an add_onX its event's args
// (impl/member.h).

#include "../core.h"

namespace wxl::impl {

// What every spelling of a member has; the four specialisations below only say which spelling
// it is. The parameters are a tuple as a list of types only: it is named, never made.
template <typename Class, typename Result, bool Const, bool Noexcept, typename... Parameters>
struct member_function_parts {
    using class_t = Class;
    using result_t = Result;
    using parameters_t = std::tuple<Parameters...>;

    template <std::size_t Index>
    using parameter_t = std::tuple_element_t<Index, parameters_t>;

    static constexpr std::size_t arity = sizeof...(Parameters);
    static constexpr bool is_const = Const;
    static constexpr bool is_noexcept = Noexcept;
};

// The primary is empty: asking it anything is an ordinary substitution failure, so what is not a
// member function, or is one ref-qualified or volatile, is simply not matched.
template <typename Member>
struct member_function_traits {
};

template <typename R, typename C, typename... P>
struct member_function_traits<R (C::*)(P...)> : member_function_parts<C, R, false, false, P...> {
};

template <typename R, typename C, typename... P>
struct member_function_traits<R (C::*)(P...) const>
    : member_function_parts<C, R, true, false, P...> {
};

template <typename R, typename C, typename... P>
struct member_function_traits<R (C::*)(P...) noexcept>
    : member_function_parts<C, R, false, true, P...> {
};

template <typename R, typename C, typename... P>
struct member_function_traits<R (C::*)(P...) const noexcept>
    : member_function_parts<C, R, true, true, P...> {
};

// The traits of a callable's call operator. A generic lambda's has no address to take, and an
// overloaded one has no single address: asking for either is a substitution failure too.
template <typename F>
using call_operator_traits = member_function_traits<decltype(&std::decay_t<F>::operator())>;

}  // namespace wxl::impl
