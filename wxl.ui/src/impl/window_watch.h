#pragma once

// The watch behind wxl::WindowWatch (CompositionWindow.h), over any window that has the
// pair of calls an event subscribes through. The application's window is CompositionWindow
// and its pair is the generated EventAdder of the event's key; a test hands in a window of
// its own, since the real one cannot be raised without WinUI.

#include "../events.h"

namespace wxl::impl {

// A member a WindowWatch can call: it takes the event's args, as a handler is handed them,
// or nothing, when they carry nothing it needs -- an event whose args are an empty Object
// says all it has to say by being raised. It returns nothing, and neither it nor handing it
// the args throws: the window raises its events from its procedure, from the delegates of its
// pointer source and from a noexcept listener of its zoom field, and an exception has
// nowhere to go from any of them. The sender is not offered: the window's events raise an
// empty one.
template <auto Listener, class Owner, class Args>
concept window_listener =
    requires(Owner& owner, EventArgsRef<Args> args) {
        { (owner.*Listener)(args) } noexcept -> std::same_as<void>;
    } || requires(Owner& owner) {
        { (owner.*Listener)() } noexcept -> std::same_as<void>;
    };

// `Window` is a handle, copied by value; `Adder` is the pair of calls for one event, as the
// generated EventAdder has it: add(window, handler) gives a token, remove(window, token)
// answers whether the token was there, args_t<Window> is what the event hands over.
//
// Neither Window's nor Adder's members are named at class scope: the type of a watch may be
// named before the generated vocabulary is included, as a wait's may (event_awaitable.h),
// and the event's args are read only where the watch is attached.
template <class Window, class Adder, auto Listener>
class window_watch {
    using owner_t = typename core::impl::member_owner<decltype(Listener)>::type;

public:
    /// A watch on no window yet: see attach().
    window_watch() noexcept {}

    /// A watch on `window` from the start, calling `owner`.
    window_watch(Window const& window, owner_t& owner) { attach(window, owner); }

    window_watch(window_watch const&) = delete;
    window_watch& operator=(window_watch const&) = delete;

    /// Takes the handler off the window, if it is on one. The window answering that the
    /// token was not there -- somebody took the handler off by it -- stops the process.
    ~window_watch() {
        if (!token_) return;
        bool const removed = Adder::remove(window_, token_);
        std::destroy_at(&window_);
        if (!removed) core::abort("wxl: a WindowWatch is no longer on its window");
    }

    /// Puts the watch on `window`: from now on each time the event is raised it calls the
    /// listener on `owner`. Once -- a second attach stops the process rather than leave the
    /// first window calling the owner or move the watch behind the back of whoever put it
    /// there.
    void attach(Window const& window, owner_t& owner) {
        using args_t = typename Adder::template args_t<Window>;
        // Checked here and not in the class: the watch is declared inside its owner, which
        // is not complete there, and a member cannot be called on it yet.
        static_assert(window_listener<Listener, owner_t, args_t>,
                      "wxl: a WindowWatch calls a member of its owner with the event's args, or "
                      "with nothing, from inside the window's procedure, which has nobody to "
                      "hand an exception to -- name a noexcept member that takes one of the two "
                      "and returns nothing");
        // The window is copied once its handler stands, and the copy does not throw: a
        // handler left on the window without the token to take it off is the one thing
        // the watch exists to rule out.
        static_assert(std::is_nothrow_copy_constructible_v<Window>);

        if (token_) core::abort("wxl: a WindowWatch is attached twice");
        auto const call = [target = &owner](Object const&,
                                            [[maybe_unused]] EventArgsRef<args_t> args) noexcept {
            if constexpr (std::is_invocable_v<decltype(Listener), owner_t&, EventArgsRef<args_t>>)
                (target->*Listener)(args);
            else
                (target->*Listener)();
        };
        EventToken const token = Adder::add(window, EventHandler<args_t>{call});
        std::construct_at(&window_, window);
        token_ = token;
    }

    /// Whether the watch is on a window.
    bool attached() const noexcept { return static_cast<bool>(token_); }

private:
    // The window lives while the token is set: CompositionWindow has no empty value -- made
    // without arguments, it makes a window -- and a flag beside the token would say the same
    // thing twice. The window's tokens count from one, so a subscription is never the empty
    // token.
    union {
        Window window_;
    };
    EventToken token_;
};

}  // namespace wxl::impl
