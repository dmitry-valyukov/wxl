module;

#include "coroutine_checks.h"

export module wxl.async:awaitable;

import :async_op;
import :cancellation;
import :coroutine_checks;
import std;

export namespace wxl::async {

template <class R>
class cancellable_awaitable;

/// What every asynchronous operation returns, and the only thing a coroutine ever
/// sees of one: `file f = co_await file::open(path);`.
///
/// It is the operation's owner -- a plain unique_ptr, because there is exactly one
/// owner and it is known: the coroutine frame that awaits it. The channels between
/// the two threads carry the same pointer borrowed.
///
/// **It is an ordinary object, and every use of it is a legal one**: awaited at once,
/// awaited after others started later, moved, or never awaited at all. What is not is
/// what is not for any object: awaiting one that has been moved from, or awaiting the
/// same one twice, which hands over a moved-from value -- a build that checks coroutines
/// stops at either. Going away with the operation still out gives the operation up
/// (`async_op::abandon`): the operation is asked to cancel, and unless it is
/// orphanable, this waits until the worker has let go of it -- so a frame unwinding past
/// a read into its own buffer is gone only once nobody writes there, the way it would be
/// on an ordinary stack.
template <class R>
class awaitable
{
public:
    explicit awaitable(std::unique_ptr<async_op_t<R>> op) noexcept : op_(std::move(op)) {}

    awaitable(awaitable&&) noexcept = default;

    awaitable& operator=(awaitable&& other) noexcept {
        if (this != &other) {
            give_up();
            op_ = std::move(other.op_);
        }

        return *this;
    }

    ~awaitable() { give_up(); }

    /// Whether a co_await would go on without suspending: the loop has taken the
    /// operation out of the return channel, and its answer -- the value or the failure
    /// -- is here. Not whether the operation has finished: one that has may still be in
    /// the channel, and until the loop takes it out it is not the coroutine's.
    bool ready([[maybe_unused]] coro_detail::site where = coro_detail::site::current()) const noexcept {
        coro_check(op_, "awaitable: moved-from", where);
        return op_->delivered();
    }

    /// Asks the operation to stop early, if it is still out (`async_op::cancel`). The
    /// co_await still ends when the operation comes back -- that is when nobody writes
    /// into the frame any more -- which is what a wait under a token waits for before it
    /// answers with the cancellation.
    void cancel() noexcept {
        if (op_ && !op_->delivered()) op_->cancel();
    }

    // Each hook is declared twice, as task's are: a strict build's takes the place of the
    // co_await, any other build's has the signature it always had.

    bool await_ready() const noexcept requires(!coro_detail::strict) { return ready(); }

    bool await_ready(coro_detail::site where = coro_detail::site::current()) const noexcept
        requires(coro_detail::strict)
    {
        return ready(where);
    }

    void await_suspend(std::coroutine_handle<> coro) noexcept { op_->suspend(coro); }

    /// \throw whatever the body threw on the worker thread: an operation that
    ///        failed fails at the co_await, where the coroutine can catch it as
    ///        its own.
    R await_resume() requires(!coro_detail::strict) { return resume(coro_detail::site::current()); }

    R await_resume(coro_detail::site where = coro_detail::site::current()) requires(coro_detail::strict) {
        return resume(where);
    }

private:
    R resume([[maybe_unused]] coro_detail::site where) {
        coro_check(op_, "awaitable: moved-from", where);
        return op_->take_result();
    }

    /// A delivered operation is the awaitable's alone, and goes with it; one still out
    /// is given up, and deleted by whoever meets it in the return channel.
    void give_up() noexcept {
        if (op_ && !op_->delivered()) async_op::abandon(op_.release());
    }

    std::unique_ptr<async_op_t<R>> op_;

    /// Which starts none at all, under a token that is cancelled already.
    friend class cancellable_awaitable<R>;
};

/// An operation under a token: what the operations of this module that can be cut short
/// return when they are given one -- `co_await async_file::read_all(path, stop)` -- and
/// what a wait under a token does (`cancellation_detail::wait_under`), with the
/// operation's awaitable made in place and the token held, not borrowed.
///
/// So it is an ordinary object, as the awaitable is: awaited at once, or kept and awaited
/// later -- the token kept with it, whatever happens to the source -- or moved while it is
/// not awaited. Under a token that is cancelled already nothing is started, and the
/// co_await ends with the cancellation at once.
template <class R>
class [[nodiscard("an operation under a token is given up at once if nobody keeps it")]] cancellable_awaitable
    : public cancellation_detail::wait_under<cancellable_awaitable<R>, true>
{
    using base = cancellation_detail::wait_under<cancellable_awaitable, true>;

    friend base;

public:
    /// \param start makes the operation's awaitable -- called here, unless `stop` is
    ///        cancelled already, and then the operation is never made.
    template <class Start>
    inline cancellable_awaitable(cancellation_token stop, Start&& start)
        : token_(std::move(stop)),
          awaitable_(token_.is_canceled() ? awaitable<R>(std::unique_ptr<async_op_t<R>>()) : start()) {}

    inline cancellable_awaitable(cancellable_awaitable&& other) noexcept
        : base(std::move(other)), token_(std::move(other.token_)), awaitable_(std::move(other.awaitable_)) {
        coro_check(!other.linked(), "cancellable_awaitable: moved while it is awaited",
                   coro_detail::site::current());
    }

    inline cancellable_awaitable& operator=(cancellable_awaitable&& other) noexcept {
        coro_check(!this->linked() && !other.linked(), "cancellable_awaitable: moved while it is awaited",
                   coro_detail::site::current());

        awaitable_ = std::move(other.awaitable_);
        token_ = std::move(other.token_);
        return *this;
    }

    /// A frame destroyed while it stands here takes the wait off the list first: the
    /// token below may be what keeps the list.
    inline ~cancellable_awaitable() { this->leave(); }

private:
    inline awaitable<R>& awaiter() noexcept { return awaitable_; }

    inline cancellation_detail::cancellation_state* state() const noexcept {
        return cancellation_detail::state_of(token_);
    }

    inline bool ready_when_told(coro_detail::site where) {
        return !awaitable_.op_ || base::ready_when_told(where);
    }

    cancellation_token token_;
    awaitable<R> awaitable_;
};

}  // export namespace wxl::async
