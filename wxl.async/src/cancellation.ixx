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
/// allocates nothing; it adds the node while it stands and removes it when it is over. A
/// child state, one per source, stands as a callback the event makes.
using told_event = core::event<void() noexcept>;

class cancellation_state;

using cancellation_state_ptr = core::intrusive_ptr<cancellation_state>;

/// What a source and its tokens share: the answer, and the event that tells it -- and, for
/// a source made under a parent token, its own place in the parent's event. Whoever stands
/// under the token subscribes and unsubscribes itself, through the event, and the state
/// knows none of them: a parent knows nothing of its children either.
///
/// One thread, the one the coroutines are on: an operation that is told sets its own
/// flag, the one the worker reads before the body anyway, so the answer is a plain field
/// and the event an STA one.
class cancellation_state : public core::sta_refcounted
{
public:
    /// A state of its own: cancelled by its source alone.
    cancellation_state() noexcept = default;

    /// A state under the state of a parent token, or under none (null): cancelled by its own
    /// source, or by the parent's -- whichever comes first. Under a live parent it stands in
    /// the parent's event as any subscriber does, a callback that cancels it, and holds the
    /// parent, so that the event outlives the stand and a chain of generations holds
    /// together; the parent holds nothing of it. Under a parent cancelled already it is born
    /// cancelled and stands nowhere.
    inline explicit cancellation_state(cancellation_state_ptr parent) {
        if (!parent) return;

        if (parent->canceled()) {
            canceled_ = true;
            return;
        }

        // By a plain pointer: the node lives in the parent's event, and a count there would
        // keep this state alive as long as the parent, and the parent as long as this one.
        subscription_ = parent->told().add([this]() noexcept { cancel(); }).get();
        parent_ = std::move(parent);
    }

    /// Takes its callback out of the parent's event: here and nowhere else -- its own
    /// cancel() keeps it standing, and the parent telling it again is cancelling it twice.
    /// One thread makes, tells and destroys a state, and telling destroys nobody, so its end
    /// never falls inside the parent's fire, and the destructor is all it takes to leave: no
    /// second count of who may still call it.
    ///
    /// A parent cancelled meanwhile has emptied its event into the one it fired and let go of
    /// the callback, so remove() finds nothing and answers false: the list compares the
    /// cookie's address and never reads through it, and since nobody subscribes under a
    /// cancelled parent, no later node there can have taken that address.
    inline ~cancellation_state() override {
        if (parent_) parent_->told().remove(*subscription_);
    }

    inline bool canceled() const noexcept { return canceled_; }

    /// The event a subscriber adds itself to and removes itself from.
    inline told_event& told() noexcept { return told_; }

    /// Sets the answer and tells everyone standing, once: the event is emptied into a local
    /// one and fired there, so a subscriber that unsubscribes later -- or from inside its
    /// call -- finds it gone, and remove() answers false. Telling resumes nobody (an
    /// operation is cut short, a wait is handed to its thread's queue), so no subscriber
    /// goes away in the middle of the fire. One that begins standing afterwards finds the
    /// answer and does not stand at all. A child state told here tells its own the same
    /// way, from inside this fire, and goes nowhere either.
    inline void cancel() noexcept {
        canceled_ = true;

        told_event once;
        once.swap(told_);
        once.fire();
    }

private:
    bool canceled_ = false;
    told_event told_;

    /// The parent's state and the cookie of the callback standing in its event, while this
    /// state stands there; both empty for a state of its own.
    cancellation_state_ptr parent_;
    core::nullable<const void> subscription_;
};

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
///   and the wait is ended, and the co_await ends with operation_canceled_exception. That
///   is the answer of what was cut short or never started; what got there first -- an
///   operation already done, an event already come -- answers with its own result. Under
///   a token cancelled already nothing is started, and the co_await ends with it at once.
/// - **The application's own coroutine** asks it itself -- is_canceled(),
///   throw_if_canceled() -- where it has something to stop, and decides what to do about
///   it. wxl asks nobody else's code to end: there is no wrapper that puts an awaiter of
///   somebody else's under a token, and no call of sta_loop that puts the application's
///   body under one -- some work must not be broken off, and only its own code knows which.
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
///
/// Only a source cancels, and a token only reads, so a part of some work that may be
/// cancelled apart from the rest has a source of its own, made under the token of the
/// whole: `cancellation_source child(parent);`. Its cancel() reaches its own tokens alone,
/// and the parent's reaches them too. This is how work is divided: each piece of it -- a
/// page being laid out, a download -- takes its own source under its owner's token, and
/// its operations stand under that. A token's event is a singly linked list that leaving
/// walks, and this keeps it a handful long: a token's own operations and waits, and the
/// sources made under it.
class cancellation_source
{
public:
    inline cancellation_source()
        : state_(core::make_refcounted<cancellation_detail::cancellation_state>()) {}

    /// A source under `parent`, cancelled by its own cancel() or by the parent's, whichever
    /// comes first. Its state stands in the parent's event, a callback that cancels it,
    /// from now until the state goes, and holds the parent's state meanwhile; under a parent
    /// cancelled already it is born cancelled, and under a token of nobody it is a source
    /// like any other. Made on the coroutines' thread, whose event it joins; a build that
    /// checks coroutines makes sure.
    inline explicit cancellation_source(
        cancellation_token parent,
        [[maybe_unused]] coro_detail::site where = coro_detail::site::current()) {
        coro_check(core::sta_memory_pool::is_safe(),
                   "cancellation_source: made under a token on a thread other than its coroutines'",
                   where);

        state_ = core::make_refcounted<cancellation_detail::cancellation_state>(
            std::move(parent.state_));
    }

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
