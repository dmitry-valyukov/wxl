module;

#include "coroutine_checks.h"

export module wxl.async:cancellation;

import :coroutine_checks;
import wxl.core;
import std;

export namespace wxl::async {

/// The operation was cancelled.
///
/// Lives here rather than with the component model that used to own it: an
/// asynchronous operation may be cancelled without any component being
/// involved, and the layer that hands out the right to cancel is the one that
/// has to name the outcome.
class operation_canceled_exception : public std::runtime_error
{
    using base = std::runtime_error;

public:
    inline operation_canceled_exception() : base("Operation was canceled") {}
    inline explicit operation_canceled_exception(const char* message) : base(message) {}
    inline explicit operation_canceled_exception(std::string_view message)
        : base(std::string(message)) {}
};

class cancellation_token;

namespace cancellation_detail {

/// What is told when a token is cancelled: an event of no arguments. Whoever stands under
/// a token -- an operation or a wait of wxl -- builds its node into itself (a member
/// deriving from `told_event::func_t`, whose release() does nothing), so that standing
/// allocates nothing; it adds the node while it stands and removes it when it is over.
using told_event = core::event<void() noexcept>;

/// What a source and its tokens share: the answer, and the event that tells it. Nothing
/// else -- whoever stands under the token subscribes and unsubscribes itself, through
/// the event, and the state knows none of them.
///
/// One thread, the one the coroutines are on: an operation that is told sets its own
/// flag, the one the worker reads before the body anyway, so the answer is a plain field
/// and the event an STA one.
class cancellation_state : public core::sta_refcounted
{
public:
    inline bool canceled() const noexcept { return canceled_; }

    /// The event a subscriber adds itself to and removes itself from.
    inline told_event& told() noexcept { return told_; }

    /// Sets the answer and tells everyone standing, once: the event is emptied into a local
    /// one and fired there, so a subscriber that unsubscribes later -- or from inside its
    /// call -- finds it gone, and remove() answers false. Telling resumes nobody (an
    /// operation is cut short, a wait is handed to its thread's queue), so no subscriber
    /// goes away in the middle of the fire. One that begins standing afterwards finds the
    /// answer and does not stand at all.
    inline void cancel() noexcept {
        canceled_ = true;

        told_event once;
        once.swap(told_);
        once.fire();
    }

private:
    bool canceled_ = false;
    told_event told_;
};

using cancellation_state_ptr = core::intrusive_ptr<cancellation_state>;

/// The state behind a token, for whoever stands on it; null for a token that has none.
cancellation_state* state_of(const cancellation_token& token) noexcept;

/// What a wait of wxl under a token asks of its awaiter: `cancel() noexcept`, which tells
/// it to end its wait early. Told, it still ends the way it ends anyway -- the event ends,
/// the thread's turn comes -- only sooner; and it is told on the coroutine's own thread,
/// while the coroutine is suspended in it or just before it would be, and resumes nobody
/// from inside the call. A told awaiter is then left without its await_resume(): whatever
/// it would have handed over is not asked for. Told before it suspends, it does not
/// suspend. A form that answers rather than throws says what a cancelled wait answers
/// through `await_canceled()`, which the wait then gives instead of the exception.
template <class Awaiter>
concept cancellable_awaiter = requires(Awaiter& awaiter) {
    { awaiter.cancel() } noexcept;
};

/// The hooks of a wait of wxl under a token, over what `Wait` gives them: `awaiter()`, the
/// awaiter of what is awaited (`cancellable_awaiter`), and `state()`, the token's state
/// or null.
///
/// Until the token is cancelled the wait is the awaiter's own, and stands under the token
/// while the coroutine is suspended: its node -- built in, nothing is allocated -- is added
/// to the token's event when it suspends and removed when it resumes. Once the token is
/// cancelled the wait ends with operation_canceled_exception: one standing is told through
/// the awaiter's cancel() and ends when the awaiter lets it; one that would begin later is
/// told before it suspends, and does not. One that answers a cancellation rather than
/// throwing it -- the answering form of an event wait -- gives its answer through
/// await_canceled().
///
/// Each hook takes the place of the co_await and hands it on to an awaiter that takes
/// one, so that a strict build reports the line of the co_await rather than this file's.
/// `Wait` leaves the event in its own destructor, before its members go: one of them may
/// be what keeps the state alive.
template <class Wait>
class wait_under
{
public:
    inline bool await_ready(coro_detail::site where = coro_detail::site::current()) {
        if (cancellation_state* const state = self().state(); state && state->canceled()) [[unlikely]]
            self().awaiter().cancel();

        return ready(where);
    }

    template <class Promise>
    inline decltype(auto) await_suspend(std::coroutine_handle<Promise> awaiting,
                                        [[maybe_unused]] coro_detail::site where = coro_detail::site::current()) {
        if (cancellation_state* const state = self().state(); state && !state->canceled()) {
            state->told().add(core::as_not_null<told_event::func_t>(&node_));
            standing_ = true;
        }

        if constexpr (requires { self().awaiter().await_suspend(awaiting, where); })
            return self().awaiter().await_suspend(awaiting, where);
        else
            return self().awaiter().await_suspend(awaiting);
    }

    inline decltype(auto) await_resume([[maybe_unused]] coro_detail::site where = coro_detail::site::current()) {
        leave();

        if (cancellation_state* const state = self().state(); state && state->canceled()) [[unlikely]] {
            if constexpr (requires { self().awaiter().await_canceled(); })
                return self().awaiter().await_canceled();
            else
                throw operation_canceled_exception();
        }

        if constexpr (requires { self().awaiter().await_resume(where); })
            return self().awaiter().await_resume(where);
        else
            return self().awaiter().await_resume();
    }

protected:
    inline wait_under() noexcept : node_(*this) {}

    ~wait_under() = default;

    /// Takes the wait out of the token's event, if it stands there.
    inline void leave() noexcept {
        if (standing_) {
            standing_ = false;
            self().state()->told().remove(core::cookie_t{static_cast<told_event::func_t*>(&node_)});
        }
    }

private:
    /// The wait's place in the token's event, built into it.
    class node final : public told_event::func_t
    {
    public:
        inline explicit node(wait_under& wait) noexcept : wait_(wait) {}

        /// The token was cancelled: the event the node stood in is gone, and the awaiter is
        /// told.
        inline void operator()() noexcept override {
            wait_.standing_ = false;
            wait_.self().awaiter().cancel();
        }

        /// The wait owns its node.
        inline void release() noexcept override {}

    private:
        wait_under& wait_;
    };

    inline Wait& self() noexcept { return static_cast<Wait&>(*this); }

    inline bool ready([[maybe_unused]] coro_detail::site where) {
        if constexpr (requires { self().awaiter().await_ready(where); })
            return self().awaiter().await_ready(where);
        else
            return self().awaiter().await_ready();
    }

    node node_;
    bool standing_ = false;
};

}  // namespace cancellation_detail

/// The right to ask the coroutines that were given it to end.
///
/// A token is passed down a chain of coroutines explicitly, as an argument, and read where
/// the chain's own code reads it. Two kinds of reader:
///
/// - **The operations and waits of wxl** that can be cut short take it as their last
///   argument -- `co_await async_file::read_all(path, stop)`, `co_await onClick(button,
///   stop)` -- and stand under it while they are out: asked, the operation is cut short
///   and the wait is ended, and the co_await ends with operation_canceled_exception. Under
///   a token cancelled already nothing is started, and the co_await ends with it at once.
/// - **The application's own coroutine** asks it itself -- is_canceled(),
///   throw_if_canceled() -- where it has something to stop, and decides what to do about
///   it. wxl asks nobody else's code to end: there is no wrapper that puts an awaiter of
///   somebody else's under a token.
///
/// Asking does not end anything by itself: the chain unwinds by its own exceptions,
/// through its own catch blocks, on the live thread -- and cleanup past a handler may
/// co_await again, under no token or another. A chain that ignores the answer runs on;
/// destroying it is still its owner's to do.
///
/// What a token does not do is interrupt a call already under way that has nothing to
/// interrupt it with: the grain is one operation or one wait.
///
/// One thread: tokens and their source live on the thread the coroutines run on. A
/// default-constructed token is never cancelled and costs nothing: it has no state,
/// and asking is a comparison against a null pointer.
class cancellation_token
{
public:
    cancellation_token() noexcept = default;

    inline bool is_canceled() const noexcept { return state_ && state_->canceled(); }

    /// \throw operation_canceled_exception if cancellation has been requested.
    inline void throw_if_canceled() const {
        if (is_canceled()) throw operation_canceled_exception();
    }

private:
    friend class cancellation_source;
    friend cancellation_detail::cancellation_state* cancellation_detail::state_of(
        const cancellation_token&) noexcept;

    inline explicit cancellation_token(cancellation_detail::cancellation_state* state) noexcept
        : state_(state) {}

    cancellation_detail::cancellation_state_ptr state_;
};

inline cancellation_detail::cancellation_state* cancellation_detail::state_of(
    const cancellation_token& token) noexcept {
    return token.state_.get();
}

/// The other end of a token: the one that asks.
///
/// The state is shared and lives as long as the last holder, so a chain may outlive
/// the source that asked it to end, and the source may outlive the chain. Asking is
/// the first of two steps, as with event waits: told, the chains end by themselves,
/// and whatever has not ended when its owner can wait no longer is destroyed by the
/// owner, which gives up its operations the usual way.
class cancellation_source
{
public:
    inline cancellation_source()
        : state_(core::make_refcounted<cancellation_detail::cancellation_state>()) {}

    inline cancellation_token token() const noexcept { return cancellation_token(state_.get()); }

    /// Tells every operation and wait standing under a token of this source, and answers
    /// every later one at once. Held across the telling: an operation told may let go of
    /// something that holds this source. On the coroutines' own thread, like everything
    /// about them; a build that checks coroutines makes sure.
    inline void cancel([[maybe_unused]] coro_detail::site where = coro_detail::site::current()) noexcept {
        coro_check(core::sta_memory_pool::is_safe(),
                   "cancellation_source: cancelled from a thread other than its coroutines'", where);

        const cancellation_detail::cancellation_state_ptr held = state_;
        held->cancel();
    }

    inline bool is_canceled() const noexcept { return state_->canceled(); }

private:
    cancellation_detail::cancellation_state_ptr state_;
};

}  // export namespace wxl::async
