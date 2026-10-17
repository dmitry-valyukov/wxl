#pragma once

// The watch behind wxl::RenderingDeviceWatch (DrawingSurface.h), over any handle that has
// the pair of calls a watch subscribes through. The application's handle is a Compositor
// and its pair is impl::rendering_device_replaced (DrawingSurface.cpp); a test hands in a
// fake of each, since a real device is replaced only when a real GPU is lost. No Windows
// header here: the public header carries this one.

#include <memory>
#include <type_traits>

#include "../events.h"

namespace wxl::impl {

// A member a RenderingDeviceWatch can call: it takes nothing -- that the device was
// replaced is all there is to say -- returns nothing, and does not throw: the replacement
// runs from a turn of the UI thread's queue, which has nobody to hand an exception to.
template <auto Listener, class Owner>
concept device_listener = requires(Owner& owner) {
    { (owner.*Listener)() } noexcept -> std::same_as<void>;
};

// `Handle` is copied by value and names whose devices are watched; `Events` is the pair of
// calls: add(handle, handler) gives a token, remove(handle, token) answers whether the
// token was there.
template <class Handle, class Events, auto Listener>
class device_watch {
    using owner_t = typename core::impl::member_owner<decltype(Listener)>::type;

public:
    /// A watch on nothing yet: see attach().
    device_watch() noexcept {}

    /// A watch on `handle` from the start, calling `owner`.
    device_watch(Handle const& handle, owner_t& owner) { attach(handle, owner); }

    device_watch(device_watch const&) = delete;
    device_watch& operator=(device_watch const&) = delete;

    /// Takes the handler off, if it is on. An answer that the token was not there --
    /// somebody took the handler off by it -- stops the process.
    ~device_watch() {
        if (!token_) return;
        bool const removed = Events::remove(handle_, token_);
        std::destroy_at(&handle_);
        if (!removed) core::abort("wxl: a RenderingDeviceWatch is no longer on its compositor");
    }

    /// Puts the watch on `handle`: from now on each time the device under its surfaces is
    /// replaced it calls the listener on `owner`. Once -- a second attach stops the process.
    void attach(Handle const& handle, owner_t& owner) {
        // Checked here and not in the class: the watch is declared inside its owner, which
        // is not complete there.
        static_assert(device_listener<Listener, owner_t>,
                      "wxl: a RenderingDeviceWatch calls a member of its owner with nothing, from "
                      "a turn of the UI thread's queue, which has nobody to hand an exception "
                      "to -- name a noexcept member that takes nothing and returns nothing");
        // The handle is copied once the handler stands, and the copy does not throw: a
        // handler left on without the token to take it off is what the watch rules out.
        static_assert(std::is_nothrow_copy_constructible_v<Handle>);

        if (token_) core::abort("wxl: a RenderingDeviceWatch is attached twice");
        auto const call = [target = &owner](Object const&, EventArgsRef<Object>) noexcept {
            (target->*Listener)();
        };
        EventToken const token = Events::add(handle, EventHandler<Object>{call});
        std::construct_at(&handle_, handle);
        token_ = token;
    }

    /// Whether the watch is on.
    bool attached() const noexcept { return static_cast<bool>(token_); }

private:
    // The handle lives while the token is set, as the window of a WindowWatch does; the
    // tokens count from one, so a subscription is never the empty token.
    union {
        Handle handle_;
    };
    EventToken token_;
};

}  // namespace wxl::impl
