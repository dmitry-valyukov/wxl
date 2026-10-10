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

/// A wait the source can reach: linked into the shared state for as long as a
/// coroutine stands in it, and taken out before it is told -- so that whatever the
/// telling sets off, a resumption included, finds the list whole.
class wait : public core::intrusive_list_node<wait>
{
public:
    using tell_t = void (*)(wait&) noexcept;

    inline explicit wait(tell_t tell) noexcept : tell_(tell) {}

    inline void tell() noexcept { tell_(*this); }

private:
    tell_t tell_;
};

/// What a source and its tokens share: the answer, and the waits standing on it.
///
/// One thread, the one the waits are on. Nothing here travels to a worker: an
/// operation that is told sets its own flag, the one the worker reads before the
/// body anyway, so the count and the answer are plain fields.
class cancellation_state : public core::sta_refcounted
{
public:
    inline bool canceled() const noexcept { return canceled_; }

    /// Tells every wait standing on this state, each taken out first. A wait that
    /// begins afterwards finds the answer and does not stand, so a second call has
    /// nobody left to tell.
    inline void cancel() noexcept {
        canceled_ = true;

        while (wait* const told = waits_.front()) {
            core::intrusive_list<wait>::remove(core::not_null<wait>(told));
            told->tell();
        }
    }

    inline void enter(wait& standing) noexcept { waits_.push_back(core::not_null<wait>(&standing)); }

private:
    bool canceled_ = false;
    core::intrusive_list<wait> waits_;
};

using cancellation_state_ptr = core::intrusive_ptr<cancellation_state>;

/// The place a wait takes in the state's list -- or none, for a wait that cannot be told
/// and so never stands in one: such a wait carries nothing of the list.
template <bool can_be_told>
class standing;

template <>
class standing<true> : public wait
{
protected:
    inline explicit standing(tell_t tell) noexcept : wait(tell) {}

    inline void leave() noexcept {
        if (linked()) core::intrusive_list<wait>::remove(core::not_null<wait>(this));
    }
};

template <>
class standing<false>
{
protected:
    inline explicit standing(wait::tell_t) noexcept {}

    inline void leave() noexcept {}
};

/// What `co_await` makes of an operand: what its member operator co_await returns,
/// else what a free one returns, else the operand itself.
template <class Awaitable>
inline decltype(auto) operand_awaiter(Awaitable&& awaitable) {
    if constexpr (requires { std::forward<Awaitable>(awaitable).operator co_await(); })
        return std::forward<Awaitable>(awaitable).operator co_await();
    else if constexpr (requires { operator co_await(std::forward<Awaitable>(awaitable)); })
        return operator co_await(std::forward<Awaitable>(awaitable));
    else
        return static_cast<std::remove_reference_t<Awaitable>&>(awaitable);
}

/// The state behind a token, for the waits that stand on it.
cancellation_state* state_of(const cancellation_token& token) noexcept;

/// The hooks of a wait under a token, over what `Wait` gives them: `awaiter()`, the
/// awaiter of what is awaited, and `state()`, the token's state or null. `told` says
/// whether that awaiter can be told (`cancellable_awaiter`).
///
/// Until the token is cancelled the wait is the awaiter's own, and stands on the token's
/// list while the coroutine is suspended -- four pointers written in, four out. Once it
/// is cancelled the wait ends with operation_canceled_exception: one standing is told
/// through the awaiter's cancel() and ends when the awaiter lets it; one that would begin
/// later does not stand at all -- unless its awaiter still has something out that
/// borrows the frame, which it is told about and waited for, without holding the thread.
/// An awaiter that cannot be told is not interrupted: a wait on it ends when it ends,
/// and then with the cancellation. One whose cancel() answers says each time whether it
/// could be told -- a task can be when an operation makes its result, and cannot when a
/// coroutine does -- and one that could not is waited on as such. A told awaiter is left
/// without its await_resume();
/// one that answers a cancellation rather than throwing it -- the answering form of an
/// event wait -- gives its answer through await_canceled().
///
/// Each hook takes the place of the co_await and hands it on to an awaiter that takes
/// one, so that a strict build reports the line of the co_await rather than this file's.
/// `Wait` leaves the list in its own destructor, before its members go: one of them may
/// be what keeps the state alive.
template <class Wait, bool told>
class wait_under : public standing<told>
{
public:
    inline bool await_ready(coro_detail::site where = coro_detail::site::current()) {
        if (cancellation_state* const state = self().state(); state && state->canceled()) [[unlikely]]
            return self().ready_when_told(where);

        return ready(where);
    }

    template <class Promise>
    inline decltype(auto) await_suspend(std::coroutine_handle<Promise> awaiting,
                                        [[maybe_unused]] coro_detail::site where = coro_detail::site::current()) {
        if constexpr (told)
            if (cancellation_state* const state = self().state(); state && !state->canceled())
                state->enter(*this);

        if constexpr (requires { self().awaiter().await_suspend(awaiting, where); })
            return self().awaiter().await_suspend(awaiting, where);
        else
            return self().awaiter().await_suspend(awaiting);
    }

    inline decltype(auto) await_resume([[maybe_unused]] coro_detail::site where = coro_detail::site::current()) {
        this->leave();

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
    inline wait_under() noexcept : standing<told>(&tell_this) {}

    /// A place of its own, in no list: a wait is moved only while it does not stand.
    inline wait_under(wait_under&&) noexcept : standing<told>(&tell_this) {}

    ~wait_under() = default;

    /// The token was cancelled before the wait stood.
    inline bool ready_when_told(coro_detail::site where) {
        if constexpr (!told) {
            return true;
        } else if constexpr (std::same_as<decltype(self().awaiter().cancel()), bool>) {
            return !self().awaiter().cancel() || ready(where);
        } else {
            self().awaiter().cancel();
            return ready(where);
        }
    }

    inline bool ready([[maybe_unused]] coro_detail::site where) {
        if constexpr (requires { self().awaiter().await_ready(where); })
            return self().awaiter().await_ready(where);
        else
            return self().awaiter().await_ready();
    }

private:
    inline Wait& self() noexcept { return static_cast<Wait&>(*this); }

    static void tell_this(wait& standing_wait) noexcept {
        if constexpr (told) static_cast<Wait&>(static_cast<wait_under&>(standing_wait)).awaiter().cancel();
    }
};

}  // namespace cancellation_detail

/// An awaiter that can be told to end its wait early. Told, it still ends the way it
/// ends anyway -- the operation comes back, the event ends -- only sooner; and it is
/// told on the coroutine's own thread, while the coroutine is suspended in it or just
/// before it would be. A told awaiter is then left without its await_resume(): whatever
/// it would have handed over is not asked for. Told before it suspends, it suspends only
/// while something it has out borrows the frame -- an operation in flight, until it comes
/// back -- and an event wait, which has nothing out, does not suspend at all. A form that
/// answers rather than throws says what a cancelled wait answers through
/// `await_canceled()`, which a wait under a token then gives instead of the exception.
/// A cancel() that answers says whether there was anybody to tell: one that answers
/// `false` is, for that wait, an awaiter that cannot be told.
template <class Awaiter>
concept cancellable_awaiter = requires(Awaiter& awaiter) {
    { awaiter.cancel() } noexcept;
};

/// The right to ask the coroutines that were given it to end.
///
/// A token is passed down a chain of coroutines explicitly, as an argument, and read at
/// the waits that name it: the operations of this module that can be cut short take one
/// as their last argument -- `co_await async_file::read_all(path, stop)` -- and any other
/// wait is put under one by `co_await cancellable(wait, stop)`. Asking does
/// not end anything by itself: a wait standing under the token ends with
/// operation_canceled_exception, and every later one ends with it at once, so the chain
/// unwinds by its own exceptions, through its own catch blocks, on the live thread --
/// and cleanup past a handler may co_await again, under no token or another. A chain
/// that ignores the answer runs on; destroying it is still its owner's to do.
///
/// What a token does not do is interrupt a call already under way that has nothing to
/// interrupt it with: the grain is one wait, and what ends a wait early is the
/// awaiter's own cancel().
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

    /// Tells every wait standing under a token of this source, and answers every later
    /// one at once. Held across the telling: a wait told may end something that holds
    /// this source. On the coroutines' own thread, like everything about them; a build
    /// that checks coroutines makes sure.
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

/// A wait under a token, for an operand that has no overload taking one:
/// `co_await cancellable(wait, token)`. What it does is `cancellation_detail::wait_under`'s.
///
/// The operand is held as `co_await` would hold it: an lvalue is borrowed, an rvalue is
/// moved in -- or, if it cannot be moved, as the awaiters of waits on this thread
/// cannot, borrowed for the full expression it was made in. The token is borrowed the
/// same way. So the wait is co_awaited where it is made, as `co_await f()` is.
template <class Awaitable>
class [[nodiscard("a wait under a token does nothing until it is co_awaited")]] cancellable_wait
    : public cancellation_detail::wait_under<
          cancellable_wait<Awaitable>,
          cancellable_awaiter<std::remove_reference_t<
              decltype(cancellation_detail::operand_awaiter(std::declval<Awaitable>()))>>>
{
    using awaiter_t = decltype(cancellation_detail::operand_awaiter(std::declval<Awaitable>()));

    using base = cancellation_detail::wait_under<
        cancellable_wait, cancellable_awaiter<std::remove_reference_t<awaiter_t>>>;

    friend base;

    using operand_t = std::conditional_t<std::move_constructible<Awaitable>, Awaitable, Awaitable&&>;

public:
    inline cancellable_wait(Awaitable&& awaitable, const cancellation_token& token)
        : operand_(std::forward<Awaitable>(awaitable)),
          awaiter_(cancellation_detail::operand_awaiter(std::forward<Awaitable>(operand_))),
          state_(cancellation_detail::state_of(token)) {}

    cancellable_wait(const cancellable_wait&) = delete;
    cancellable_wait& operator=(const cancellable_wait&) = delete;

    /// A frame destroyed while it stands here takes the wait off the list.
    inline ~cancellable_wait() { this->leave(); }

private:
    inline std::remove_reference_t<awaiter_t>& awaiter() noexcept { return awaiter_; }

    inline cancellation_detail::cancellation_state* state() const noexcept { return state_; }

    operand_t operand_;
    awaiter_t awaiter_;

    /// Borrowed from the token, which outlives the full expression the wait is made in.
    cancellation_detail::cancellation_state* state_;
};

/// \return the wait for `awaitable` under `token`, to be co_awaited in the same
///         expression: `co_await cancellable(file.read(buf), stop)`.
template <class Awaitable>
[[nodiscard]] inline cancellable_wait<Awaitable> cancellable(Awaitable&& awaitable,
                                                             const cancellation_token& token) {
    return cancellable_wait<Awaitable>(std::forward<Awaitable>(awaitable), token);
}

}  // export namespace wxl::async
