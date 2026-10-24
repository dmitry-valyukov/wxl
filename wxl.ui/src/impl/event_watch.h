#pragma once

// The watch behind wxl::WindowWatch (CompositionWindow.h) and wxl::RenderingDeviceWatch
// (DrawingSurface.h): a member of its owner, called each time an event of a source is raised,
// over any source that has the pair of calls an event subscribes through. The window's pair is
// the generated event_adder of the event's key; a compositor's is impl::rendering_device_replaced
// (DrawingSurface.cpp). A test hands in a source of its own, since the real ones cannot be raised
// without WinUI or a lost GPU. No Windows header here: the public headers carry this one.

#include <memory>
#include <type_traits>

#include "../events.h"

namespace wxl::impl {

// A member an event watch can call: it takes the event's args, as a handler is handed them, or
// nothing, when they carry nothing it needs -- an event whose args are an empty Object says all
// it has to say by being raised. It returns nothing, and neither it nor handing it the args
// throws: a window raises its events from its procedure, from the delegates of its pointer
// source and from a noexcept listener of its zoom field, and a device is replaced from a turn of
// the UI thread's queue; an exception has nowhere to go from any of them. The sender is not
// offered: the sources raise an empty one.
template <auto Listener, class Owner, class Args>
concept event_listener =
    requires(Owner& owner, EventArgsRef<Args> args) {
        { (owner.*Listener)(args) } noexcept -> std::same_as<void>;
    } || requires(Owner& owner) {
        { (owner.*Listener)() } noexcept -> std::same_as<void>;
    };

// `Source` is a handle, copied by value; `Adder` is the pair of calls for one event, as the
// generated event_adder has it: add(source, handler) gives a token, remove(source, token) takes
// the watch's token by reference, empties it and answers whether it was there, args_t<Source> is
// what the event hands over.
//
// Neither Source's nor Adder's members are named at class scope: the type of a watch may be
// named before the generated vocabulary is included, as a wait's may (event_awaitable.h), and
// the event's args are read only where the watch is attached.
template <class Source, class Adder, auto Listener>
class event_watch {
    using owner_t = typename core::impl::member_owner<decltype(Listener)>::type;

public:
    /// A watch on no source yet: see attach().
    event_watch() noexcept {}

    /// A watch on `source` from the start, calling `owner`.
    event_watch(Source const& source, owner_t& owner) { attach(source, owner); }

    event_watch(event_watch const&) = delete;
    event_watch& operator=(event_watch const&) = delete;

    /// Takes the handler off the source, if it is on one. The source answering that the token
    /// was not there -- somebody took the handler off by it -- stops the process.
    ~event_watch() {
        if (!token_) return;
        bool const removed = Adder::remove(source_, token_);
        std::destroy_at(&source_);
        if (!removed) core::abort("wxl: an event watch is no longer on its source");
    }

    /// Puts the watch on `source`: from now on each time the event is raised it calls the
    /// listener on `owner`. Once -- a second attach stops the process rather than leave the
    /// first source calling the owner or move the watch behind the back of whoever put it there.
    void attach(Source const& source, owner_t& owner) {
        using args_t = typename Adder::template args_t<Source>;
        // Checked here and not in the class: the watch is declared inside its owner, which is
        // not complete there, and a member cannot be called on it yet.
        static_assert(event_listener<Listener, owner_t, args_t>,
                      "wxl: an event watch calls a member of its owner with the event's args, or "
                      "with nothing, from where the source raises it, which has nobody to hand an "
                      "exception to -- name a noexcept member that takes one of the two and "
                      "returns nothing");
        // The source is copied once its handler stands, and the copy does not throw: a handler
        // left on the source without the token to take it off is the one thing the watch exists
        // to rule out.
        static_assert(std::is_nothrow_copy_constructible_v<Source>);

        if (token_) core::abort("wxl: an event watch is attached twice");
        auto const call = [target = &owner](Object const&,
                                            [[maybe_unused]] EventArgsRef<args_t> args) noexcept {
            if constexpr (std::is_invocable_v<decltype(Listener), owner_t&, EventArgsRef<args_t>>)
                (target->*Listener)(args);
            else
                (target->*Listener)();
        };
        EventToken token = Adder::add(source, EventHandler<args_t>{call});
        std::construct_at(&source_, source);
        token_ = std::move(token);
    }

    /// Whether the watch is on a source.
    bool attached() const noexcept { return static_cast<bool>(token_); }

private:
    // The source lives while the token is set: CompositionWindow has no empty value -- made
    // without arguments, it makes a window -- and a flag beside the token would say the same
    // thing twice. The tokens count from one, so a subscription is never the empty token.
    union {
        Source source_;
    };
    EventToken token_;
};

}  // namespace wxl::impl
