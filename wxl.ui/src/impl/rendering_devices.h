#pragma once

// The rendering device every graphics device of wxl draws through, those graphics devices -- one
// per compositor -- and the order in which the device under them is replaced (DrawingSurface.cpp),
// over a backend a test can fake: a real device is replaced only when a real GPU is lost. No
// Windows header here; the backend brings its own.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "handler_list.h"

namespace wxl::impl {

/// Which composition API made a graphics device: WinUI's (Microsoft.UI.Composition), whose
/// compositors every DrawingSurface is made on, or the system's (Windows.UI.Composition), on
/// which WindowShade flies a window outside any window. Their interop is the same calls under
/// IIDs of their own, so a graphics device is moved through the interface of its API.
enum class composition_api { winui, system };

/// The device in use, the graphics devices made on it and the holders -- the handlers of the
/// watches -- of each compositor; and the replacement, which runs in this order: the old device's
/// watch for removal taken back, a new device made, every graphics device moved onto it, the old
/// one let go -- and only then every holder called, so that nothing an owner draws from its call
/// lands on the old device.
///
/// Each device is made under a generation of its own, counted from one, and the watch for its
/// removal asks for the replacement of that generation: an answer to a device replaced already
/// replaces nothing.
///
/// A graphics device is not made on a lost device. Asked for one while the device in use says it
/// was removed, before the answer of its removal has come, the class moves everything onto a new
/// device right there and calls nobody: the call that asked is somebody else's -- a DrawingSurface
/// being made, from inside a holder's own call as well -- and a holder called from inside it would
/// make graphics devices and surfaces under that caller's feet. The holders are called from a turn
/// of the UI thread's queue posted there (notify()); until one has run, or a replacement after it
/// has called them, holders_told() is false, and every draw() answers false: what the owners made
/// from the old device is not dropped yet.
///
/// A compositor's entry goes once it has neither a graphics device nor a holder, and its identity
/// is let go -- never while the holders are being called: a holder may take a watch off from its
/// call, and the calls go through the entries by index.
///
/// Once the thread's queue begins to shut down (close()), nothing is replaced and nobody is called
/// any more: the compositors close with the frameworks on the way out, and a graphics device of a
/// closed compositor refuses a new rendering device.
///
/// `Backend` makes and moves the real things:
///   owner_t     what names a compositor: its COM identity, compared and never called through
///   device_t    a rendering device with its watch for removal
///   graphics_t  a graphics device; a value-initialized one is none
///   device_t make(std::uint64_t generation)                  a device, watched for removal --
///                                                            the watch asks for replace() of
///                                                            that generation
///   bool lost(device_t const&) noexcept                      whether it says it was removed
///   void unwatch(device_t&) noexcept                         its watch taken back
///   void release(device_t&) noexcept                         the device let go
///   void move(graphics_t, composition_api, device_t const&)  a graphics device onto the device
///   void post() noexcept                                     a turn of the UI thread's queue
///                                                            that calls notify()
///   void keep(owner_t) noexcept                              the identity held while its entry
///                                                            stands, so that a later compositor
///                                                            cannot take its address
///   void forget(owner_t) noexcept                            the identity let go with its entry
///
/// One thread, the UI thread: every call, the replacement included, is made there.
template <class Backend>
class rendering_devices {
public:
    using owner_t = typename Backend::owner_t;
    using device_t = typename Backend::device_t;
    using graphics_t = typename Backend::graphics_t;

    explicit rendering_devices(Backend backend = Backend{}) : backend_(std::move(backend)) {}

    rendering_devices(rendering_devices const&) = delete;
    rendering_devices& operator=(rendering_devices const&) = delete;

    Backend& backend() noexcept { return backend_; }

    /// The device in use; null before the first is made.
    device_t const* current() const noexcept { return made_ ? &device_ : nullptr; }

    /// How many times the device has been replaced -- counted when everything is moved, before any
    /// holder is called, so that what a holder asks for finds the count of the device that stands.
    std::uint64_t replacements() const noexcept { return made_ ? generation_ - 1 : 0; }

    /// Whether the holders have been called for the device in use. False from a replacement that
    /// graphics_of() ran until its turn: drawing then would put what an owner made from the old
    /// device into a context of the new one.
    bool holders_told() const noexcept { return told_ == generation_; }

    /// Whether the replacement is over: the thread's queue has begun to shut down.
    bool closed() const noexcept { return closed_; }

    /// The device in use; the first is made here -- after close(), with no watch for its removal.
    device_t const& device() {
        if (!made_) {
            device_ = backend_.make(generation_ + 1);
            ++generation_;
            told_ = generation_;
            made_ = true;
            if (closed_) backend_.unwatch(device_);
        }
        return device_;
    }

    /// The graphics device of the compositor `owner` names, made by `make(device)` the first time
    /// it is asked for -- on the device in use, unless that device says it was removed: then
    /// everything is moved onto a new device here, and the turn that calls the holders is posted;
    /// the answer of the removal, when it comes, finds its device gone.
    template <class Make>
    graphics_t graphics_of(owner_t owner, composition_api api, Make&& make) {
        std::size_t const at = index_of(owner, api);
        if (!entries_[at].graphics) {
            if (made_ && !closed_ && backend_.lost(device_)) {
                move();
                backend_.post();
            }
            entries_[at].graphics = std::forward<Make>(make)(device());
        }
        return entries_[at].graphics;
    }

    /// The holders of the compositor `owner` names. Its entry is made here, with no graphics device
    /// yet, for a watch put on before the compositor's first surface.
    handler_list<Object>& handlers_of(owner_t owner, composition_api api) {
        return *entries_[index_of(owner, api)].replaced;
    }

    /// Takes the holder `token` names off the compositor `owner` names; false when it was not
    /// there -- the compositor has no entry, or the token is not of its list. An entry left with
    /// neither a graphics device nor a holder goes here, or once the holders' calls are over.
    bool remove(owner_t owner, EventToken token) {
        for (entry& each : entries_) {
            if (each.owner != owner) continue;
            bool const removed = each.replaced->remove(token);
            if (removed && calling_ == 0) sweep();
            return removed;
        }
        return false;
    }

    /// Replaces the device of generation `lost`, in the order the class gives -- if it is still
    /// the one in use, and the thread is not shutting down; otherwise there is nothing to do.
    /// noexcept: it runs from a turn of the UI thread's queue, and a graphics device that refuses
    /// the new device leaves nothing to draw with -- the exception stops the process, named by
    /// wxl's terminate handler.
    void replace(std::uint64_t lost) noexcept {
        if (closed_ || !made_ || lost != generation_) return;
        move();
        tell();
    }

    /// The turn graphics_of() posted: the holders are called for the device in use -- unless they
    /// were called for it already, by a replacement or by an earlier turn (one posted for a device
    /// replaced again since finds the newest one), or the thread is shutting down.
    void notify() noexcept {
        if (closed_ || told_ == generation_) return;
        tell();
    }

    /// The thread's queue begins to shut down: the device in use is no longer watched, and nothing
    /// is replaced from here on -- an answer of a removal finds the replacement over, posted before
    /// or after, by a callback still under way, and so does a turn of the holders. The device stays, to draw on until the end: a lost one,
    /// or one whose holders were never called, answers false to every draw().
    void close() noexcept {
        if (closed_) return;
        closed_ = true;
        if (made_) backend_.unwatch(device_);
    }

private:
    struct entry {
        owner_t owner;
        composition_api api;
        graphics_t graphics;
        // On the heap: a holder called from the list may add an entry, and the vector moves.
        std::unique_ptr<handler_list<Object>> replaced;
    };

    /// The entry of `owner`, made if there is none. A compositor is of one API; asked for under
    /// the other, it is a mistake of wxl's own, and stops.
    std::size_t index_of(owner_t owner, composition_api api) {
        for (std::size_t at = 0; at < entries_.size(); ++at) {
            if (entries_[at].owner != owner) continue;
            if (entries_[at].api != api) core::abort("wxl: one compositor is asked for under two composition APIs");
            return at;
        }
        backend_.keep(owner);
        entries_.push_back({owner, api, graphics_t{}, std::make_unique<handler_list<Object>>()});
        return entries_.size() - 1;
    }

    /// The device in use for a new one, every graphics device moved onto it, the old one let go;
    /// nobody is called. noexcept for the reason replace() gives.
    void move() noexcept {
        backend_.unwatch(device_);
        device_t old = std::exchange(device_, backend_.make(generation_ + 1));
        ++generation_;
        for (entry const& each : entries_) {
            if (each.graphics) backend_.move(each.graphics, each.api, device_);
        }
        backend_.release(old);
    }

    /// Every holder called, for the device in use. Told before the first call, so that a holder
    /// drawing again from its call draws. By index and by the count before the first call: a holder
    /// drawing again may be the first to draw on another compositor and add an entry -- made on
    /// the new device already, so it has nothing to be told -- and the entries emptied by the calls
    /// go once the last of them is over.
    void tell() noexcept {
        told_ = generation_;
        ++calling_;
        std::size_t const known = entries_.size();
        for (std::size_t at = 0; at < known; ++at) entries_[at].replaced->fire(Object::Impl::empty<Object>());
        if (--calling_ == 0) sweep();
    }

    /// The entries with neither a graphics device nor a holder go, with their identities.
    void sweep() noexcept {
        std::erase_if(entries_, [this](entry const& each) {
            if (each.graphics || !each.replaced->empty()) return false;
            backend_.forget(each.owner);
            return true;
        });
    }

    Backend backend_;
    device_t device_{};
    bool made_ = false;
    bool closed_ = false;
    std::uint64_t generation_ = 0;   // of the device in use; 0 before the first
    std::uint64_t told_ = 0;         // the generation the holders were last called for
    int calling_ = 0;                // tell() under way, one inside another included
    std::vector<entry> entries_;
};

}  // namespace wxl::impl
