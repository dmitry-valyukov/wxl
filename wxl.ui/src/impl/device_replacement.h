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

/// The device in use, the graphics devices made on it and the handlers of the watches on each
/// compositor; and the replacement, which runs in this order: the old device's watch for removal
/// taken back, a new device made, every graphics device moved onto it, the old one let go -- and
/// only then every watch called, so that nothing an owner draws from its call lands on the old
/// device.
///
/// Each device is made under a generation of its own, counted from one, and the watch for its
/// removal asks for the replacement of that generation: an answer to a device replaced already --
/// by the UI thread, which found it lost before the answer came -- replaces nothing. A graphics
/// device is not made on a lost device: the replacement runs first, there. And once the thread's
/// queue begins to shut down (close()), nothing is replaced any more: the compositors close with
/// the frameworks on the way out, and a graphics device of a closed compositor refuses a new
/// rendering device.
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
///   void keep(owner_t) noexcept                              the identity held for good, so that
///                                                            a later compositor cannot take its
///                                                            address
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

    /// How many times the device has been replaced -- counted before the watches are called, so
    /// that what one of them asks for finds the count of the device that stands.
    std::uint64_t replacements() const noexcept { return made_ ? generation_ - 1 : 0; }

    /// Whether the replacement is over: the thread's queue has begun to shut down.
    bool closed() const noexcept { return closed_; }

    /// The device in use; the first is made here -- after close(), with no watch for its removal.
    device_t const& device() {
        if (!made_) {
            device_ = backend_.make(generation_ + 1);
            ++generation_;
            made_ = true;
            if (closed_) backend_.unwatch(device_);
        }
        return device_;
    }

    /// The graphics device of the compositor `owner` names, made by `make(device)` the first time
    /// it is asked for -- on the device in use, unless that device says it was removed: then the
    /// replacement its removal asked for runs here, watches and all, and the graphics device is
    /// made on the new one; the answer of the removal, when it comes, finds its device gone.
    template <class Make>
    graphics_t graphics_of(owner_t owner, composition_api api, Make&& make) {
        std::size_t const at = index_of(owner, api);
        if (!entries_[at].graphics) {
            if (made_ && !closed_ && backend_.lost(device_)) replace(generation_);
            // By index again: a watch called by the replacement may have added an entry.
            entries_[at].graphics = std::forward<Make>(make)(device());
        }
        return entries_[at].graphics;
    }

    /// The handlers of the watches on the compositor `owner` names. Its entry is made here, with
    /// no graphics device yet, for a watch put on before the compositor's first surface.
    handler_list<Object>& handlers_of(owner_t owner, composition_api api) {
        return *entries_[index_of(owner, api)].replaced;
    }

    /// Those handlers, or null for a compositor nobody has asked for.
    handler_list<Object>* find_handlers(owner_t owner) const noexcept {
        for (entry const& each : entries_) {
            if (each.owner == owner) return each.replaced.get();
        }
        return nullptr;
    }

    /// Replaces the device of generation `lost`, in the order the class gives -- if it is still
    /// the one in use, and the thread is not shutting down; otherwise there is nothing to do.
    /// Graphics devices and watches are taken by index and by the count before the first call: a
    /// watch drawing again may be the first to draw on another compositor and add an entry -- made
    /// on the new device already, so it has nothing to be moved or told. noexcept: it runs from a
    /// turn of the UI thread's queue, and a graphics device that refuses the new device leaves
    /// nothing to draw with -- the exception stops the process, named by wxl's terminate handler.
    void replace(std::uint64_t lost) noexcept {
        if (closed_ || !made_ || lost != generation_) return;

        backend_.unwatch(device_);
        device_t old = std::exchange(device_, backend_.make(generation_ + 1));
        ++generation_;

        std::size_t const known = entries_.size();
        for (std::size_t at = 0; at < known; ++at) {
            if (entries_[at].graphics) backend_.move(entries_[at].graphics, entries_[at].api, device_);
        }
        backend_.release(old);

        for (std::size_t at = 0; at < known; ++at) entries_[at].replaced->fire(Object::Impl::empty<Object>());
    }

    /// The thread's queue begins to shut down: the device in use is no longer watched, and nothing
    /// is replaced from here on -- an answer of a removal posted before finds the replacement over.
    /// The device stays, to draw on until the end: a lost one answers false to every draw().
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
        // On the heap: a watch called from the list may add an entry, and the vector moves.
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

    Backend backend_;
    device_t device_{};
    bool made_ = false;
    bool closed_ = false;
    std::uint64_t generation_ = 0;   // of the device in use; 0 before the first
    std::vector<entry> entries_;
};

}  // namespace wxl::impl
