#pragma once

// The watch behind wxl::WindowWatch (CompositionWindow.h) and wxl::RenderingDeviceWatch
// (DrawingSurface.h): a member of its owner, called each time an event of a source is raised,
// over any source that has the pair of calls an event subscribes through. The window's pair is
// the generated event_adder of the event's key; a compositor's is impl::rendering_device_replaced
// (DrawingSurface.cpp). A test hands in a source of its own, since the real ones cannot be raised
// without WinUI or a lost GPU. No Windows header here: the public headers carry this one.

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
// the event's args are read only where the watch is made.
//
// There is no watch on nothing: one is made on its source, from the start of its owner's life
// -- a member built in the owner's initializer list -- and is on it until it goes. A watch that
// has to wait for its source is a part of its owner that appears then, built whole.
template <class Source, class Adder, auto Listener>
class event_watch {
    using owner_t = typename core::impl::member_owner<decltype(Listener)>::type;

public:
    /// A watch on `source`, calling the listener on `owner` each time the event is raised. The
    /// source is copied first and the handler put on it after: a subscription that throws
    /// leaves no handler behind, and the copy goes with the half-made watch.
    event_watch(Source const& source, owner_t& owner) : source_(source), token_(subscribe(owner)) {}

    event_watch(event_watch const&) = delete;
    event_watch& operator=(event_watch const&) = delete;

    /// Takes the handler off the source. The source answering that the token was not there --
    /// somebody took the handler off by it -- stops the process.
    ~event_watch() {
        if (!Adder::remove(source_, token_)) core::abort("wxl: an event watch is no longer on its source");
    }

private:
    EventToken subscribe(owner_t& owner) {
        using args_t = typename Adder::template args_t<Source>;
        // Checked here and not in the class: the watch is declared inside its owner, which is
        // not complete there, and a member cannot be called on it yet.
        static_assert(event_listener<Listener, owner_t, args_t>,
                      "wxl: an event watch calls a member of its owner with the event's args, or "
                      "with nothing, from where the source raises it, which has nobody to hand an "
                      "exception to -- name a noexcept member that takes one of the two and "
                      "returns nothing");
        auto const call = [target = &owner](Object const&,
                                            [[maybe_unused]] EventArgsRef<args_t> args) noexcept {
            if constexpr (std::is_invocable_v<decltype(Listener), owner_t&, EventArgsRef<args_t>>)
                (target->*Listener)(args);
            else
                (target->*Listener)();
        };
        return Adder::add(source_, EventHandler<args_t>{call});
    }

    Source const source_;
    EventToken token_;
};

}  // namespace wxl::impl
