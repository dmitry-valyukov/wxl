module;

#include "abi.h"

export module wxl.async:async_op;

import wxl.core;
import std;

export namespace wxl::async {

/// One asynchronous operation: the body that runs on the worker thread, what it
/// leaves behind, and the coroutine that is waiting for it.
///
/// Its life is a round trip. It is born on the STA thread, inside the call that
/// starts the operation; it travels to the worker as a plain pointer, is
/// executed there, and travels back the same way; and it dies on the STA thread,
/// when the co_await that was waiting for it ends. The worker in between neither
/// creates nor destroys it -- which is exactly why the memory comes from
/// sta_memory_pool: both ends of the life are on the pool's own thread.
///
/// Ownership is the awaiting coroutine's: the `awaitable` holding this op sits in
/// the coroutine frame, and the two channels carry a borrowed pointer. So a frame
/// destroyed while an operation is still out there takes the operation's memory
/// with it -- do not drop a `task` that has not finished.
class async_op : public core::noncopyable
{
public:
    virtual ~async_op() = default;

    /// Created and destroyed on the STA thread, both, so the pool is where it
    /// belongs. The destructor is virtual, so `delete` through this base reaches
    /// the most derived type's deallocation and hands the pool the size it
    /// actually gave out.
    inline static void* operator new(std::size_t size) {
        return core::sta_memory_pool::alloc(size);
    }

    inline static void operator delete(void* mem, std::size_t size) noexcept {
        core::sta_memory_pool::free(mem, size);
    }

    /// The worker thread's call: runs the body and keeps whatever it threw, since
    /// there is nobody here to throw to -- the exception belongs to the coroutine
    /// and travels back to it in the op itself.
    ///
    /// \return `true` if the operation is finished and goes back to the STA
    ///         thread. A body that is not done answers `false`, and the operation
    ///         goes nowhere: it now belongs to whatever it is waiting for -- the
    ///         OS, a readiness notification, another queue -- and comes back here
    ///         when that fires. Which means this is not called once per operation:
    ///         a socket that has taken only part of a message is executed again,
    ///         and a body has to be written knowing it.
    inline bool packaged_execute() noexcept {
        try {
            return execute();
        } catch (...) {
            error_ = std::current_exception();
            return true;
        }
    }

    /// Remembers the coroutine to hand control back to. The STA thread's call, made
    /// from await_suspend() -- that is, after the op may already be sitting in the
    /// return channel, and before anything can take it out of there, because taking
    /// it out is the same thread's job.
    inline void suspend(std::coroutine_handle<> coro) noexcept { coro_ = coro; }

    /// The STA thread's call: gives control back to the coroutine that awaited this
    /// op.
    ///
    /// \warning The op is gone by the time this returns. The coroutine resumes
    ///          inside its co_await, and the awaitable holding the op -- and the op
    ///          with it -- is destroyed as that expression ends.
    inline void resume() const { coro_.resume(); }

    inline bool has_exception() const noexcept { return error_ != nullptr; }

protected:
    /// The work itself, on the worker thread. \see packaged_execute().
    virtual bool execute() = 0;

    inline void rethrow_if_failed() const {
        if (error_) std::rethrow_exception(error_);
    }

private:
    std::coroutine_handle<> coro_;
    std::exception_ptr error_;
};

/// An operation with a result of type R: the value the worker produced, kept until
/// the coroutine comes back for it.
template <class R>
class async_op_t : public async_op
{
public:
    /// The STA thread's call, at the end of co_await.
    /// \throw whatever the body threw on the worker thread.
    R take_result() {
        rethrow_if_failed();

        // Finished, did not throw, and left nothing behind: that is not a
        // failure the caller can do anything about, it is a body that answered
        // "done" without producing what it promised.
        ensure(value_.has_value() && "async_op: finished without a result");

        return std::move(*value_);
    }

protected:
    void set_value(R&& value) { value_.emplace(std::move(value)); }

private:
    // An optional rather than an R: a result type is not obliged to have a default
    // constructor, and an open file has no sensible empty state anyway.
    std::optional<R> value_;
};

template <>
class async_op_t<void> : public async_op
{
public:
    inline void take_result() { rethrow_if_failed(); }
};

/// The simple case: the body is a lambda or a functor, and the result is whatever
/// it returns.
///
/// It is held by value rather than behind a core::function, because the op is
/// allocated as itself anyway and a second allocation would buy nothing.
template <class Fn, class R = std::invoke_result_t<Fn&>>
class async_op_f : public async_op_t<R>
{
public:
    /// Forwarding, so the callable is built here out of what the caller wrote
    /// and is not copied on the way. It matters more than it looks: a lambda
    /// that has captured a path carries a pool allocation in its capture, and a
    /// copy on the way in would be one more of them per operation.
    template <class Fn2>
    explicit async_op_f(Fn2&& fn) : fn_(std::forward<Fn2>(fn)) {}

protected:
    bool execute() override {
        if constexpr (std::is_void_v<R>)
            fn_();
        else
            this->set_value(fn_());

        return true;
    }

private:
    Fn fn_;
};

}  // export namespace wxl::async
