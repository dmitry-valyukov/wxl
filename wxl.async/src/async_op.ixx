module;

#include "abi.h"

export module wxl.async:async_op;

import wxl.core;
import std;

export namespace wxl::async {

/// Says of an operation that it touches nothing but what it owns -- no buffer in a
/// coroutine frame, no object the coroutine holds -- so that an awaitable going away early
/// may leave it to finish alone instead of waiting for it. Opening a file is the model: it
/// carries its own copy of the path, and what it brings back, the open file, is released
/// by whoever ends up holding it.
struct orphanable_t {
    explicit orphanable_t() = default;
};

inline constexpr orphanable_t orphanable{};

/// One asynchronous operation: the body that runs on the worker thread, what it
/// leaves behind, and the coroutine that is waiting for it.
///
/// Its life is a round trip. It is born on the STA thread, inside the call that
/// starts the operation; it travels to the worker as a plain pointer, is
/// executed there, and travels back the same way; and it dies on the STA thread.
/// The worker in between neither creates nor destroys it -- which is exactly why
/// the memory comes from sta_memory_pool: both ends of the life are on the pool's
/// own thread.
///
/// Ownership is the `awaitable`'s, which sits in the coroutine frame, while the two
/// channels carry a borrowed pointer. So the awaitable cannot simply delete an
/// operation that is still out: when it goes away first -- its frame unwinding on an
/// exception, its task dropped, or nobody ever awaiting it -- it gives the operation up
/// instead. The operation is asked to cancel, the awaitable waits until the worker has
/// let go of it (unless it is `orphanable`), and the loop deletes it when it comes
/// back. That wait is what keeps a buffer in the frame from being written after the
/// frame is gone -- the same guarantee a synchronous read on an ordinary stack gives.
class async_op : public core::noncopyable
{
public:
    async_op() noexcept = default;

    inline explicit async_op(orphanable_t) noexcept : orphanable_(true) {}

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
    /// An operation given up before the worker reached it is not started: nobody is
    /// waiting for what it would bring, and the flag is read here, once, with an
    /// ordinary load.
    ///
    /// \return `true` if the operation is finished and goes back to the STA
    ///         thread. A body that is not done answers `false`, and the operation
    ///         goes nowhere: it now belongs to whatever it is waiting for -- the
    ///         OS, a readiness notification, another queue -- and comes back here
    ///         when that fires. Which means this is not called once per operation:
    ///         a socket that has taken only part of a message is executed again,
    ///         and a body has to be written knowing it. Nor may it fail to come
    ///         back: an awaitable giving it up waits for it.
    inline bool packaged_execute() noexcept {
        if (canceled_.load(std::memory_order_relaxed)) [[unlikely]]
            return true;

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

    /// The STA thread's call, as the loop takes the operation out of the return channel:
    /// deletes it if it was given up, resumes the coroutine waiting for it if there is
    /// one, and otherwise leaves it for the co_await still to come, which then finds it
    /// back and does not suspend.
    ///
    /// \return whether a coroutine was resumed.
    ///
    /// \warning The op may be gone by the time this returns: a resumed coroutine goes
    ///          on from its co_await, and the awaitable holding the op dies with it.
    inline bool come_back() {
        if (abandoned_) [[unlikely]] {
            delete this;
            return false;
        }

        stage_ = stage::delivered;

        if (!coro_) return false;

        coro_.resume();
        return true;
    }

    /// The same, for a loop that is being stopped: nothing is resumed any more, so a
    /// coroutine suspended here stays where it is, and the op waits for its awaitable
    /// to delete it along with the frame.
    inline void settle() noexcept {
        if (abandoned_)
            delete this;
        else
            stage_ = stage::delivered;
    }

    /// Whether the loop has taken it out of the return channel: the worker is done
    /// with it, and nothing but its awaitable refers to it any more.
    inline bool delivered() const noexcept { return stage_ == stage::delivered; }

    /// Whether a wait looking ahead in the return channel has passed it there: the
    /// worker is done with it, though it has not been taken out yet.
    inline bool seen() const noexcept { return stage_ == stage::seen; }

    /// The looking wait's call, for every operation it passes. \see seen()
    inline void mark_seen() noexcept { stage_ = stage::seen; }

    /// The awaitable's call, when it goes away before the operation has been delivered:
    /// marks it given up, asks it to cancel, and -- unless it is orphanable -- waits,
    /// without resuming anybody, until the worker has let go of it. The loop deletes it
    /// when it comes back, or this does, if it is next in line already.
    ///
    /// Defined with the loop, whose return channel it waits on (sta_loop.cpp).
    static void abandon(async_op* op) noexcept;

    inline bool has_exception() const noexcept { return error_ != nullptr; }

protected:
    /// The work itself, on the worker thread. \see packaged_execute().
    virtual bool execute() = 0;

    /// Called on the STA thread when the awaitable gives the operation up while it is
    /// still out, after the flag packaged_execute() reads has been set.
    ///
    /// For an operation the worker has not reached, the flag is all it takes. One already
    /// running is interrupted here, if what it waits for can be interrupted -- the way
    /// CancelIoEx completes a read the kernel is holding -- so that the awaitable, which
    /// waits for it to come back, does not wait longer than it has to. Must not throw:
    /// it runs in a destructor, often one called by unwinding.
    virtual void on_cancel() noexcept {}

    inline void rethrow_if_failed() const {
        if (error_) std::rethrow_exception(error_);
    }

private:
    /// Where the operation is, as the STA thread knows it. The worker never reads it.
    enum class stage : std::uint8_t {
        /// Handed to the worker, and not seen back yet.
        sent,
        /// Seen in the return channel by a wait looking ahead in it: the worker is done
        /// with it, and it has not been taken out yet.
        seen,
        /// Taken out of the return channel.
        delivered,
    };

    std::coroutine_handle<> coro_;
    std::exception_ptr error_;

    /// Written by the STA thread, read by the worker before the body: the one field
    /// of the operation both threads touch while it is out, hence atomic.
    std::atomic<bool> canceled_{false};

    stage stage_ = stage::sent;

    /// Given up by its awaitable: whoever takes it out of the return channel deletes it.
    bool abandoned_ = false;

    const bool orphanable_ = false;
};

/// An operation with a result of type R: the value the worker produced, kept until
/// the coroutine comes back for it.
template <class R>
class async_op_t : public async_op
{
public:
    using async_op::async_op;

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
    using async_op::async_op;

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

    template <class Fn2>
    async_op_f(orphanable_t tag, Fn2&& fn) : async_op_t<R>(tag), fn_(std::forward<Fn2>(fn)) {}

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
