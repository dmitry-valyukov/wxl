module;

#include "coroutine_checks.h"

export module wxl.async:awaitable;

import :async_op;
import :coroutine_checks;
import std;

export namespace wxl::async {

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
};

}  // export namespace wxl::async
