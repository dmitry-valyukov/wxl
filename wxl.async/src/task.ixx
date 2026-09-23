export module wxl.async:task;

import wxl.core;
import std;

export namespace wxl::async {

/// A coroutine somebody holds: it is handed back to its caller to be kept,
/// swept and read.
///
/// That is the whole difference from `detached_task`, which owns itself and
/// answers to nobody. Here the caller keeps what comes back -- the Reader's
/// `Io` keeps them in a vector and sweeps the finished ones -- because there
/// is something to read at the end, and, while an operation of this module is
/// in flight, because there is something the worker still points at.
///
/// It starts running the moment it is called (initial_suspend is
/// suspend_never), on the calling thread: an asynchronous operation is created
/// where the caller stands, and everything between two co_awaits runs there and
/// nowhere else. What each co_await does with the work in between -- send it to
/// another thread, or merely give the thread back to its own event loop -- is
/// the awaitable's business, not this one's. So what sets it apart from
/// `detached_task` is ownership, not where the work goes.
///
/// At the end the coroutine does not disappear (final_suspend is
/// suspend_always): the frame is what the owner asks done() and takes the
/// exception from. The frame is destroyed by the task, and what that
/// means while the coroutine is still suspended depends on what it is
/// suspended on.
///
/// **Dropping an unfinished one is safe only where the awaitable can take
/// itself back.** Destroying the frame destroys its locals and the awaiter it
/// stands in, and nothing else happens: nobody is resumed, and no result is
/// ever taken. For an awaitable that is one end of a subscription on this same
/// thread -- wxl.ui's event proxy, whose awaiter unhooks in its destructor
/// -- that is the ordinary way such a coroutine is stopped, and the only one:
/// an endless loop over an event has no other end. For the asynchronous
/// operations in this module it is a use-after-free, because the worker holds
/// the frame's own async_op borrowed through the channel and will write into
/// it after the frame is gone. So one awaiting those must be held until
/// done(), and one awaiting only events on its own thread may be let go
/// whenever its owner is.
///
/// **The frame comes from sta_memory_pool.** It is exactly what that pool is
/// for -- a small object, made and unmade on the one thread, over and over --
/// and it means the thread this coroutine belongs to has to be the pool's
/// thread, with the pool built before the first coroutine and outliving the
/// last. That is not a restriction this type adds: everything else in this
/// scheme is allocated there too, and a coroutine on any other thread would
/// have nowhere to put its operations anyway.
class task
{
public:
    struct promise_type {
        inline task get_return_object() {
            return task(std::coroutine_handle<promise_type>::from_promise(*this));
        }

        /// The frame, from the pool. The sized form of the deallocation is the one
        /// the compiler calls for a coroutine frame, so the pool gets back the very
        /// size it handed out and never has to be asked to remember it.
        inline static void* operator new(std::size_t size) {
            return core::sta_memory_pool::alloc(size);
        }

        inline static void operator delete(void* mem, std::size_t size) noexcept {
            core::sta_memory_pool::free(mem, size);
        }

        inline std::suspend_never initial_suspend() const noexcept { return {}; }
        inline std::suspend_always final_suspend() const noexcept { return {}; }

        inline void return_void() const noexcept {}

        inline void unhandled_exception() noexcept { error = std::current_exception(); }

        std::exception_ptr error;
    };

    inline task(task&& other) noexcept
        : handle_(std::exchange(other.handle_, {})) {}

    inline task& operator=(task&& other) noexcept {
        std::swap(handle_, other.handle_);
        return *this;
    }

    inline ~task() {
        if (handle_) handle_.destroy();
    }

    /// \return `true` once the coroutine has run to its end, whether by
    ///         reaching it or by leaving through an exception.
    inline bool done() const noexcept { return handle_.done(); }

    /// \throw whatever left the coroutine. Ask after done().
    inline void result() const {
        if (const std::exception_ptr& error = handle_.promise().error)
            std::rethrow_exception(error);
    }

private:
    inline explicit task(std::coroutine_handle<promise_type> handle) noexcept
        : handle_(handle) {}

    std::coroutine_handle<promise_type> handle_;
};

}  // export namespace wxl::async
