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
// Its own header, not events.h, because it names async::detached_task: the
// import of wxl.async stays with the files that write coroutines, instead of
// reaching every translation unit that includes a wrapper.

#include "core.h"

import wxl.async;

namespace wxl {

namespace impl {

// The call method() returns: an object, one of its members, and a call
// operator that is not a template but takes the member's parameters as they
// are declared. A coroutine member's token is dropped here; method() admits
// no other result.
template <typename Obj, typename Member, typename... Params>
class method_call {
    static_assert(std::is_invocable_v<Member, Obj*, Params...>,
                  "wxl: method() was given a member this object cannot call. Through a pointer to "
                  "const -- `this` inside a const member function -- only a const member can be "
                  "named; otherwise the member belongs to another class.");

public:
    constexpr method_call(Obj* object, Member member) noexcept : object_{object}, member_{member} {}

    constexpr void operator()(Params... params) const {
        static_cast<void>((object_->*member_)(std::forward<Params>(params)...));
    }

private:
    Obj* object_;
    Member member_;
};

// A member pointer taken apart into its result and the call over its
// parameters. The four spellings a handler member has -- const or not,
// noexcept or not -- the same four events.h reads off a call operator; a
// ref-qualified or volatile member has no specialisation and stops here.
template <typename Member>
struct member_signature;

template <typename R, typename C, typename... P>
struct member_signature<R (C::*)(P...)> {
    using result_t = R;
    template <typename Obj>
    using call_t = method_call<Obj, R (C::*)(P...), P...>;
};
template <typename R, typename C, typename... P>
struct member_signature<R (C::*)(P...) const> {
    using result_t = R;
    template <typename Obj>
    using call_t = method_call<Obj, R (C::*)(P...) const, P...>;
};
template <typename R, typename C, typename... P>
struct member_signature<R (C::*)(P...) noexcept> {
    using result_t = R;
    template <typename Obj>
    using call_t = method_call<Obj, R (C::*)(P...) noexcept, P...>;
};
template <typename R, typename C, typename... P>
struct member_signature<R (C::*)(P...) const noexcept> {
    using result_t = R;
    template <typename Obj>
    using call_t = method_call<Obj, R (C::*)(P...) const noexcept, P...>;
};

// What the caller of a handler may drop without losing anything: no result
// at all, or the token of a coroutine that started on the call and owns
// itself.
template <typename R>
concept droppable_result = std::is_void_v<R> || std::is_same_v<R, async::detached_task>;

}  // namespace impl

/// A member function of \p object as a callable with the member's own
/// parameters: `onKeyDown = method(this, &Screen::searchKeyDown)` in the
/// braces, `box.add_onKeyDown(method(this, &Screen::searchKeyDown))` after
/// them, `queue.tryEnqueue(method(this, &Screen::refresh))` for a delegate
/// parameter.
///
/// The call operator is not a template, so a handler's form is read off the
/// member exactly as off a lambda: the sender and the args
/// (`void searchKeyDown(TextBox const&, KeyRoutedEventArgs&)`), the sender
/// alone, or nothing; `const` and `noexcept` members are taken as they are.
/// Through a pointer to const -- `this` inside a const member function -- only
/// a const member can be named.
///
/// The member returns nothing, or `async::detached_task`: a coroutine member
/// is started by the call and its token dropped, since it owns itself, and
/// what it keeps past its first `co_await` it takes by value -- the sender and
/// the args are lent for the call only. Any other result would be lost
/// unread, and is refused.
///
/// The object is held by a bare pointer, for an object that owns the element
/// it handles: the element and its handler go before it, while a counted
/// reference would close the cycle object -> element -> handler -> object that
/// nothing ever breaks.
template <typename Obj, typename Member>
    requires std::is_member_function_pointer_v<Member>
[[nodiscard]] constexpr auto method(Obj* object, Member member) noexcept {
    using signature = impl::member_signature<Member>;
    static_assert(impl::droppable_result<typename signature::result_t>,
                  "wxl: method() takes a member that returns void, or async::detached_task when "
                  "it is a coroutine that runs on by itself. Any other result -- a value, a task "
                  "somebody has to hold -- would be dropped unread by whoever calls the handler.");
    using call = typename signature::template call_t<Obj>;
    return call{object, member};
}

}  // namespace wxl
