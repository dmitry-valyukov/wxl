#pragma once

// The public shape of an event subscription: what a handler is, and what it
// is handed besides the sender.

#include "Object.h"
#include "event_token.h"

#include <concepts>
#include <functional>
#include <type_traits>
#include <utility>

namespace wxl {

// The root of the event-args hierarchy, and the reason it is separate from
// the wrapper hierarchy: an args object belongs to the runtime, exists only
// for the duration of the callback, and is never stored or copied by the
// handler. So it is a non-owning view of the ABI pointer -- no reference
// count, no Impl allocation on a path that can fire on every mouse move --
// and non-copyable, which is what makes "only valid inside the handler"
// something the compiler enforces rather than something a comment asks for.
class EventArgsBase {
public:
    EventArgsBase(EventArgsBase const&) = delete;
    EventArgsBase& operator=(EventArgsBase const&) = delete;

protected:
    explicit EventArgsBase(void* abi) noexcept : abi_(abi) {}

    // The wrapped args object, as the raw ABI pointer the runtime passed.
    // Members read from it by querying the interface they need; there is
    // nothing to release, since the caller owns the object.
    void* abi_;

    friend class Object::Impl;
};

// How the args reach a handler, which follows from what they are.
//
// An EventArgs view is non-copyable and belongs to the runtime for the
// duration of the call, so it arrives by non-const reference: an args object
// may legitimately be written to -- a handler setting Handled, a cancellable
// request being refused -- and non-const costs nothing here, since there is
// no copy of it to be made or kept.
//
// An event whose args are a plain object hands over an ordinary wrapper
// instead, and that is a smart pointer with a copy constructor and an
// assignment operator. It arrives by *const* reference, exactly like the
// sender: non-const would offer to replace the caller's object, which is not
// something a handler may do.
template <typename Args>
using EventArgsRef = std::conditional_t<std::derived_from<Args, EventArgsBase>, Args&,
                                        Args const&>;

namespace impl {

// The sender parameter of a two-argument handler's call operator, decayed.
// The primary is `void` -- an operator() this cannot read (a different arity,
// or a generic lambda whose address cannot be taken) names no sender -- so the
// alias below is well-formed for any callable at all.
template <typename>
struct handler_sender {
    using type = void;
};

template <typename R, typename C, typename S, typename A>
struct handler_sender<R (C::*)(S, A) const> {
    using type = std::decay_t<S>;
};
template <typename R, typename C, typename S, typename A>
struct handler_sender<R (C::*)(S, A)> {
    using type = std::decay_t<S>;
};
template <typename R, typename C, typename S, typename A>
struct handler_sender<R (C::*)(S, A) const noexcept> {
    using type = std::decay_t<S>;
};
template <typename R, typename C, typename S, typename A>
struct handler_sender<R (C::*)(S, A) noexcept> {
    using type = std::decay_t<S>;
};

// The same, for a handler that takes the sender alone. Arity is what tells the
// two apart, and a one-argument callable naming anything else is simply not a
// wrapper, so the concepts below stay disjoint without saying so.
template <typename R, typename C, typename S>
struct handler_sender<R (C::*)(S) const> {
    using type = std::decay_t<S>;
};
template <typename R, typename C, typename S>
struct handler_sender<R (C::*)(S)> {
    using type = std::decay_t<S>;
};
template <typename R, typename C, typename S>
struct handler_sender<R (C::*)(S) const noexcept> {
    using type = std::decay_t<S>;
};
template <typename R, typename C, typename S>
struct handler_sender<R (C::*)(S) noexcept> {
    using type = std::decay_t<S>;
};

template <typename F, typename = void>
struct handler_sender_of {
    using type = void;  // no call operator whose address can be taken
};
template <typename F>
struct handler_sender_of<F, std::void_t<decltype(&std::decay_t<F>::operator())>> {
    using type = typename handler_sender<decltype(&std::decay_t<F>::operator())>::type;
};

// The sender a handler names, or void when it names none: a lambda taking two
// arguments has one, an overloaded/templated/other-arity callable has not.
template <typename F>
using handler_sender_t = typename handler_sender_of<F>::type;

}  // namespace impl

// A handler that names the control it expects as its sender -- (ToggleSwitch
// const&, Args) rather than (Object const&, Args) -- and is callable that way,
// and is not already a plain Object-sender handler (which is taken as it is).
// The conjunction short-circuits: the invocable-with-the-named-sender check is
// reached only once that sender is known to be a wrapper, never with void.
template <typename Fn, typename Args>
concept TypedSenderHandler =
    !std::is_invocable_v<Fn const&, Object const&, EventArgsRef<Args>>
    && std::derived_from<impl::handler_sender_t<Fn>, Object>
    && std::is_invocable_v<Fn const&, impl::handler_sender_t<Fn> const&, EventArgsRef<Args>>;


// A handler that wants the control and nothing else -- `[](Button const& self)
// { ... }`. Most events carry args a handler never reads, and a parameter
// written only to be ignored is noise; arity alone tells this form from the
// one above, so neither concept has to exclude the other.
template <typename Fn, typename Args>
concept SenderOnlyHandler =
    !std::is_invocable_v<Fn const&, Object const&, EventArgsRef<Args>>
    && std::derived_from<impl::handler_sender_t<Fn>, Object>
    && std::is_invocable_v<Fn const&, impl::handler_sender_t<Fn> const&>;

// A handler that wants neither -- `onClick = [type, symbol] { type(symbol); }`.
// What is left when everything it needs is in its own capture, and the
// alternative is two parameters written only to be ignored. Arity tells this
// form from the two above as well: a callable taking nothing names no sender,
// so it fails their second clause and they fail its last one.
template <typename Fn, typename Args>
concept NullaryHandler =
    !std::is_invocable_v<Fn const&, Object const&, EventArgsRef<Args>>
    && std::is_invocable_v<Fn const&>;

// A handler is an ordinary callable (Object const& sender, Args). The sender
// arrives as a wrapper, i.e. a smart pointer with an assignment operator, so by
// value and by const reference are equally safe -- which is exactly why every
// member of every wrapper is const-qualified.
//
// A handler may also name the control it expects instead of the base Object --
// `[](ToggleSwitch const& toggle, RoutedEventArgs&) { ... }` -- and it is then
// adapted to read that control from the bare sender by try_as. This is the way
// a handler gets the typed control that fired *without capturing it*: a control
// is a temporary in the description tree, so a captured wrapper would keep a
// dead one alive and a captured pointer would dangle, while the sender is the
// live object every time the event fires.
//
// And it may take that control alone -- `[](Button const& self) { ... }` --
// when the args say nothing it needs, which is most events; or take nothing
// at all -- `[type, symbol] { type(symbol); }` -- when it carries what it
// needs in its capture.
template <typename Args>
class EventHandler : public std::function<void(Object const&, EventArgsRef<Args>)> {
    using base_t = std::function<void(Object const&, EventArgsRef<Args>)>;

public:
    // What this handler is handed, named on the type rather than left to be
    // read off the template argument: a subscription that is not written at
    // the call site -- one made from an EventKey rather than from a name --
    // reaches the args through the signature of the object's own add_onX,
    // and a member pointer can be decomposed only into the handler type.
    using args_t = Args;

    EventHandler() = default;

    // The plain form: anything callable as (Object const&, Args). std::function's
    // own converting constructor takes it, and refuses a typed-sender handler
    // (it is not callable with an Object sender), so the two never collide.
    using base_t::base_t;

    // The typed-sender form, adapted. The sender of a control's own event is
    // that control, so a try_as miss is a wiring mistake -- try_as asserts on it
    // in a debug build -- and the handler is quietly skipped rather than called
    // on an empty wrapper.
    template <typename Fn>
        requires TypedSenderHandler<std::decay_t<Fn>, Args>
    EventHandler(Fn&& fn)
        : base_t{[fn = std::forward<Fn>(fn)](Object const& sender, EventArgsRef<Args> args) {
              using T = impl::handler_sender_t<std::decay_t<Fn>>;
              if (T const control = sender.try_as<T>()) fn(control, args);
          }} {}

    // The sender-only form, adapted the same way. The args the event carries
    // are dropped here rather than at every call site that does not read them.
    template <typename Fn>
        requires SenderOnlyHandler<std::decay_t<Fn>, Args>
    EventHandler(Fn&& fn)
        : base_t{[fn = std::forward<Fn>(fn)](Object const& sender, EventArgsRef<Args>) {
              using T = impl::handler_sender_t<std::decay_t<Fn>>;
              if (T const control = sender.try_as<T>()) fn(control);
          }} {}

    // The nullary form: both are dropped, and nothing is looked up on the way
    // in -- there is no try_as here, because there is no sender to convert.
    template <typename Fn>
        requires NullaryHandler<std::decay_t<Fn>, Args>
    EventHandler(Fn&& fn)
        : base_t{[fn = std::forward<Fn>(fn)](Object const&, EventArgsRef<Args>) { fn(); }} {}
};

}  // namespace wxl
