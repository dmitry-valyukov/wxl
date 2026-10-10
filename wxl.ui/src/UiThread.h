#pragma once

#include <coroutine>
#include <memory>

#include <wxl/Microsoft.UI.Dispatching.h>
#include "impl/event_waits.h"

// wxl::UiThread -- when work runs on the UI thread: post() hands it over from
// another thread, and onIdle() lets a coroutine on the UI thread step aside for
// what is more urgent.
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
//
// `co_await UiThread::onIdle()` hands the UI thread back and resumes once it has
// nothing more urgent: input, layout and rendering run first. Long work on the
// UI thread is cut into steps this way, without a second thread:
//
//     async::detached_task index(Model& model) {
//         while (model.step()) co_await UiThread::onIdle();
//     }
//
// Going down ends the wait the way it ends an event wait: the coroutine
// resumes with async::operation_canceled_exception. So does a cancellation
// token given last -- `co_await UiThread::onIdle(stop)` -- once it is
// cancelled: the step in progress ends on the queue's next turn, without
// waiting for the thread to be idle, and every later one ends at once.

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

    class Idle;

    /// Awaited on the UI thread: resumes once the thread has nothing more
    /// urgent to do.
    [[nodiscard]] static Idle onIdle();

    /// The same under a cancellation token: once it is cancelled the wait
    /// ends with async::operation_canceled_exception.
    [[nodiscard]] static impl::event_wait_under<Idle, async::cancellation_token> onIdle(
        async::cancellation_token stop);

private:
    ::IInspectable* queue_ = nullptr;
};

class UiThread::Idle : impl::waiting_event {
public:
    Idle() = default;
    ~Idle();

    Idle(Idle const&) = delete;
    Idle& operator=(Idle const&) = delete;

    bool await_ready() const noexcept { return skipped_ || impl::events_closing(); }
    bool await_suspend(std::coroutine_handle<> waiter);

    void await_resume() const {
        if (skipped_ || ended() || impl::events_closing()) {
            throw_ended();
        }
    }

    /// Cancelled by the token of onIdle(stop)
    /// (async::cancellation_detail::cancellable_awaiter): a wait that is
    /// suspended leaves its idle turn -- which finds nobody when it comes -- and
    /// is resumed by the queue's next turn of any priority; one that has not
    /// begun never suspends.
    void cancel() noexcept {
        if (waiting()) {
            *self_ = nullptr;
            impl::waiting_event::cancel();
        } else {
            skipped_ = true;
        }
    }

private:
    // The queued resumption finds the wait through this; a wait ended on the
    // way down, or cancelled, is gone by the time the queue gets to it, and
    // leaves it empty.
    std::shared_ptr<Idle*> self_;

    // Ended without suspending: the queue takes nothing more, or the wait was
    // cancelled before it began.
    bool skipped_ = false;
};

inline UiThread::Idle UiThread::onIdle() {
    return {};
}

inline impl::event_wait_under<UiThread::Idle, async::cancellation_token> UiThread::onIdle(
    async::cancellation_token stop) {
    return impl::event_wait_under<Idle, async::cancellation_token>{std::move(stop)};
}

}  // namespace wxl
