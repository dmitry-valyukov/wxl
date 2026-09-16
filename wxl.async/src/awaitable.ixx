export module wxl.async:awaitable;

import :async_op;
import std;

export namespace wxl::async {

/// What every asynchronous operation returns, and the only thing a coroutine ever
/// sees of one: `file f = co_await file::open(path);`.
///
/// It is the operation's owner -- a plain unique_ptr, because there is exactly one
/// owner and it is known: the coroutine frame that awaits it. The channels between
/// the two threads carry the same pointer borrowed, and nothing there outlives the
/// co_await, so there is nothing for a reference count to arbitrate.
template <class R>
class awaitable
{
public:
    explicit awaitable(std::unique_ptr<async_op_t<R>> op) : op_(std::move(op)) {}

    /// Always suspends, even when the worker has already finished.
    ///
    /// It is not an optimisation left undone. The operation is handed to the worker
    /// the moment it is created, so by the time the co_await is reached it may
    /// already be sitting in the return channel; and the only thing that takes it
    /// out of there is this very thread's loop, which resumes whatever coroutine
    /// the op names. Answering "ready" here would leave that entry with no
    /// coroutine to name.
    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> coro) noexcept { op_->suspend(coro); }

    /// \throw whatever the body threw on the worker thread: an operation that
    ///        failed fails at the co_await, where the coroutine can catch it as
    ///        its own.
    R await_resume() { return op_->take_result(); }

private:
    std::unique_ptr<async_op_t<R>> op_;
};

}  // export namespace wxl::async
