#pragma once

#include <coroutine>

#include <wxl/Microsoft.UI.Dispatching.h>
#include "impl/event_waits.h"

// wxl::UiThread -- when work runs on the UI thread: post() hands it over from
// another thread, and onIdle() lets a coroutine on the UI thread step aside for
// what is more urgent. isCurrent() and ensureCurrent() tell whether the calling
// thread is that thread.
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
//     ui.post([found = std::move(files)] { fileList.add(found); });   // anywhere
//
// The work is copied into the queue, as it must be: it runs after post() has
// returned, so nothing it needs may be held by reference.
//
// `co_await UiThread::onIdle()` hands the UI thread back and resumes once it has
// nothing more urgent: input, layout and rendering run first. Long work on the
// UI thread is split into steps this way, without a second thread:
//
//     async::task<> index(Model& model) {
//         while (model.step()) co_await UiThread::onIdle();
//     }
//
//     model.indexing = index(model);   // a field of whoever stops it
//
// Going down ends the wait the way it ends an event wait: the coroutine
// resumes with async::operation_canceled_exception. So does a cancellation
// token given last -- `co_await UiThread::onIdle(token)` -- once it is
// cancelled: the step in progress ends on the queue's next turn, without
// waiting for the thread to be idle, and every later one ends at once.

// Declared, not included: what is kept is one COM pointer, and an application
// that posts work should not have to parse a Windows header to do it.
struct IInspectable;

namespace wxl {

class UiThread
{
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
    [[nodiscard]] static impl::cancelable_event_wait<Idle, async::cancellation_token> onIdle(
        async::cancellation_token token);

    /// Whether the calling thread is the UI thread: the one wxl runs on, whose STA pool it is.
    static bool isCurrent() noexcept { return core::sta_memory_pool::is_safe(); }

    /// Stops the process when the calling thread is not the UI thread: a call from another
    /// thread to what lives on the UI thread is a broken rule. The report names the caller's line.
    static void ensureCurrent(
        std::source_location where = std::source_location::current()) noexcept {
        if (!isCurrent()) [[unlikely]]
            core::abort("called from a thread other than the UI thread", where);
    }

private:
    ::IInspectable* queue_ = nullptr;
};

namespace impl {

/// Where the queued resumption of an idle wait finds the wait: the wait while it is suspended, null
/// once it has ended on the way down, been cancelled or gone. The queue's turn runs on the wait's
/// own thread, and a queue that refuses it lets go of it there too, so the count is that thread's.
struct idle_link final : core::sta_refcounted {
    explicit idle_link(UiThread::Idle* idle) noexcept : wait(idle) {}

    UiThread::Idle* wait;
};

}  // namespace impl

class UiThread::Idle : impl::event_wait
{
public:
    Idle() = default;
    ~Idle();

    Idle(Idle const&) = delete;
    Idle& operator=(Idle const&) = delete;

    bool await_ready() const noexcept { return skipped_ || impl::event_wait::closing(); }
    bool await_suspend(std::coroutine_handle<> waiter);

    void await_resume() const {
        if (skipped_ || ended() || impl::event_wait::closing()) {
            throw_ended();
        }
    }

    /// Cancelled by the token of onIdle(token)
    /// (async::detail::cancelable_awaiter): a wait that is
    /// suspended leaves its idle turn -- which finds nobody when it comes -- and
    /// is resumed by the queue's next turn of any priority; one that has not
    /// begun never suspends.
    void cancel() noexcept {
        if (waiting()) {
            self_->wait = nullptr;
            impl::event_wait::cancel();
        } else {
            skipped_ = true;
        }
    }

private:
    // The queued resumption finds the wait through this; a wait ended on the
    // way down, or cancelled, is gone by the time the queue gets to it, and
    // leaves it empty.
    core::intrusive_ptr<impl::idle_link> self_;

    // Ended without suspending: the queue takes nothing more, or the wait was
    // cancelled before it began.
    bool skipped_ = false;
};

inline UiThread::Idle UiThread::onIdle() { return {}; }

inline impl::cancelable_event_wait<UiThread::Idle, async::cancellation_token> UiThread::onIdle(
    async::cancellation_token token) {
    return impl::cancelable_event_wait<Idle, async::cancellation_token>{std::move(token)};
}

}  // namespace wxl
