#pragma once

#include "generated/Microsoft.UI.Dispatching.h"

// wxl::UiThread -- the UI thread as seen from another one.
//
// DispatcherQueue.tryEnqueue already takes a lambda: a delegate parameter is
// projected as std::function of the delegate's own signature, so handing work
// to the UI thread reads the way it should. What it does not do is survive
// being called from somewhere else -- and somewhere else is the only place
// that ever wants it.
//
// The reason is the wrapper, not the queue. Every wxl wrapper is a handle to
// an Impl that is reference-counted without interlocked operations and
// allocated from the STA pool, and its interface cache fills in lazily on
// first use. All three are single-threaded by construction: copying a wrapper
// on a background thread races the count, and a first call there races the
// cache. The queue underneath is agile and has none of these problems.
//
// So this is that queue, and nothing else: constructed on the UI thread from
// the wrapper, held by a background thread, called from it.
//
//     UiThread const ui{window.dispatcherQueue()};   // on the UI thread
//     ...
//     ui.post([found = std::move(books)] { shelf.add(found); });   // anywhere
//
// The work is copied into the queue, as it must be: it runs after post() has
// returned, so nothing it needs may be held by reference.

// Declared, not included: what is kept is one COM pointer, and an application
// that posts work should not have to parse a Windows header to do it.
struct IInspectable;

namespace wxl {

class UiThread {
public:
    /// An empty handle: post() does nothing and says so. Exists so that an
    /// object can hold one before the window is up.
    UiThread() noexcept = default;

    explicit UiThread(DispatcherQueue const& queue);

    UiThread(UiThread const& other) noexcept;
    UiThread(UiThread&& other) noexcept;
    UiThread& operator=(UiThread const& other) noexcept;
    UiThread& operator=(UiThread&& other) noexcept;
    ~UiThread();

    explicit operator bool() const noexcept { return queue_ != nullptr; }

    /// Runs `work` on the UI thread. False means it will not run: either this
    /// handle is empty, or the queue is shutting down and takes nothing more.
    bool post(std::function<void()> work) const;

private:
    ::IInspectable* queue_ = nullptr;
};

}  // namespace wxl
