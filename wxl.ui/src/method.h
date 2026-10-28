#pragma once

// wxl::method -- a member function handed over where a callable is taken, an
// event handler above all, with the member's own parameters.
//
// A handler's form is read off the call operator of what it is given: the
// sender it names, and how many arguments it takes (events.h). A lambda that
// only forwards to a member has that operator, a member pointer has none, and
// a generic forwarder has one whose address cannot be taken. What method()
// returns has exactly the member's operator, so it takes whichever form the
// member is written in, and nothing in between has to know about it.
//
// Its own header, not events.h: it brings the import of wxl.async, which the
// files handing members over -- members that start the tasks their objects
// keep -- use, instead of reaching every translation unit that includes a
// wrapper.

#include "core.h"
#include "impl/member_function_traits.h"

import wxl.async;

namespace wxl {

namespace impl {

// What a call made by method() holds: an object and one of its members, and
// the check that the one can call the other, made here once for both
// spellings of the call below.
template <typename Obj, typename Member, typename... Params>
class method_target
{
    static_assert(std::is_invocable_v<Member, Obj*, Params...>,
                  "wxl: method() was given a member this object cannot call. Through a pointer to "
                  "const -- `this` inside a const member function -- only a const member can be "
                  "named; otherwise the member belongs to another class.");

public:
    constexpr method_target(Obj* object, Member member) noexcept
        : object_{object}, member_{member} {}

protected:
    Obj* object_;
    Member member_;
};

// The call method() returns: a call operator that is not a template but takes
// the member's parameters as they are declared. A coroutine member's token is
// dropped here; method() admits no other result.
//
// `NoExcept` picks one of two operators, each spelled out, rather than one
// with a dependent `noexcept(NoExcept)`: the handler forms and core::function
// read the operator's type off its address, so it is kept a concrete
// spelling, the way wxl.core's `impl::sig` keeps a signature.
template <typename Obj, typename Member, bool NoExcept, typename... Params>
class method_call : public method_target<Obj, Member, Params...>
{
public:
    using method_target<Obj, Member, Params...>::method_target;

    constexpr void operator()(Params... params) const {
        static_cast<void>((this->object_->*this->member_)(std::forward<Params>(params)...));
    }
};

template <typename Obj, typename Member, typename... Params>
class method_call<Obj, Member, true, Params...> : public method_target<Obj, Member, Params...>
{
public:
    using method_target<Obj, Member, Params...>::method_target;

    constexpr void operator()(Params... params) const noexcept {
        static_cast<void>((this->object_->*this->member_)(std::forward<Params>(params)...));
    }
};

// What the caller of a handler may drop without losing anything: no result
// at all.
template <typename R>
concept droppable_result = std::is_void_v<R>;

}  // namespace impl

/// A member function of \p object as a callable with the member's own
/// parameters: `onKeyDown += method(this, &Screen::searchKeyDown)` in the
/// braces, `box.add_onKeyDown(method(this, &Screen::searchKeyDown))` after
/// them, `queue.tryEnqueue(method(this, &Screen::refresh))` for a delegate
/// parameter.
///
/// The call operator is not a template, so a handler's form is read off the
/// member exactly as off a lambda: the sender and the args
/// (`void searchKeyDown(TextBox const&, KeyRoutedEventArgs&)`), the sender
/// alone, or nothing; `const` and `noexcept` members are taken as they are.
/// The call of a `noexcept` member is `noexcept` too, so a member can watch a
/// field: `observable::on_change`, like any `core::event`, takes only a
/// callback that cannot throw.
/// Through a pointer to const -- `this` inside a const member function -- only
/// a const member can be named.
///
/// The member returns nothing. One that starts a coroutine keeps its task in a
/// field of its object, which stops it by going, and passes it by value what
/// the coroutine keeps past its first `co_await` -- the sender and the args are
/// lent for the call only. Any other result would be lost unread, and is
/// refused.
///
/// The object is held by a bare pointer, for an object that owns the element
/// it handles: the element and its handler go before it, while a counted
/// reference would close the cycle object -> element -> handler -> object that
/// nothing ever breaks.
template <typename Obj, typename Member>
    requires std::is_member_function_pointer_v<Member>
[[nodiscard]] constexpr auto method(Obj* object, Member member) noexcept {
    // A ref-qualified or volatile member has no traits and stops here.
    using traits = impl::member_function_traits<Member>;
    static_assert(
        impl::droppable_result<typename traits::result_t>,
        "wxl: method() takes a member that returns void. Any other result -- a value, a task "
        "somebody has to hold -- would be dropped unread by whoever calls the handler.");
    // A noexcept member makes a noexcept call, unless handing it its arguments can throw: a
    // parameter taken by value is moved on from the call's own, and a move may throw.
    return [&]<typename... P>(std::type_identity<std::tuple<P...>>) {
        constexpr bool nothrow =
            traits::is_noexcept && std::is_nothrow_invocable_v<Member, Obj*, P...>;
        return impl::method_call<Obj, Member, nothrow, P...>{object, member};
    }(std::type_identity<typename traits::parameters_t>{});
}

}  // namespace wxl
