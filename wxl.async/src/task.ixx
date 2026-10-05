export module wxl.async:task;

import wxl.core;
import std;

export namespace wxl::async {

template <class T = void>
class task;

namespace task_detail {

/// What every task's promise has in common: the frame from the pool, the
/// start on the calling thread, the exception kept for whoever reads the
/// task, and the way out -- which hands the thread to the coroutine awaiting
/// this one, if there is one.
struct promise_base {
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

    /// The frame stays -- nothing here resumes it, the owner destroys it -- and
    /// the thread goes on to the coroutine that co_awaited this one, by
    /// symmetric transfer, so that a chain of awaiting coroutines finishes one
    /// after another without the stack growing by a frame per link. When
    /// nobody awaits, the thread goes back to whoever resumed this coroutine.
    inline auto final_suspend() noexcept {
        struct awaiter {
            std::coroutine_handle<> next;

            inline bool await_ready() const noexcept { return false; }
            inline std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept {
                return next;
            }
            inline void await_resume() const noexcept {}
        };

        return awaiter{continuation ? continuation : std::noop_coroutine()};
    }

    inline void unhandled_exception() noexcept { error = std::current_exception(); }

    /// The coroutine awaiting this one, parked by task::await_suspend; empty
    /// while nobody is. One at most: a task is awaited once, like any awaitable.
    std::coroutine_handle<> continuation;
    std::exception_ptr error;
};

template <class T>
struct value_promise : promise_base {
    task<T> get_return_object();

    /// By value rather than by forwarding reference: `co_return {}` and
    /// `co_return name_of_a_local` both have to work, and the move this costs an
    /// rvalue is nothing next to the operation the coroutine just awaited.
    inline void return_value(T value) { result.emplace(std::move(value)); }

    std::optional<T> result;
};

struct void_promise : promise_base {
    task<void> get_return_object();

    inline void return_void() const noexcept {}
};

/// The handle and what every task does with it: owns the frame, answers
/// done(), and lets another coroutine await it.
template <class Promise>
class owner {
public:
    inline owner(owner&& other) noexcept
        : handle_(std::exchange(other.handle_, {})) {}

    inline owner& operator=(owner&& other) noexcept {
        std::swap(handle_, other.handle_);
        return *this;
    }

    inline ~owner() {
        if (handle_) handle_.destroy();
    }

    /// \return `true` once the coroutine has run to its end, whether by
    ///         reaching it or by leaving through an exception.
    inline bool done() const noexcept { return handle_.done(); }

    /// A task that has already ended is awaited without suspending.
    inline bool await_ready() const noexcept { return handle_.done(); }

    /// Parks the awaiting coroutine for final_suspend to hand the thread to.
    /// Nothing else happens: this coroutine is already running, or suspended
    /// on an operation that will resume it on this same thread.
    inline void await_suspend(std::coroutine_handle<> awaiting) noexcept {
        handle_.promise().continuation = awaiting;
    }

protected:
    inline explicit owner(std::coroutine_handle<Promise> handle) noexcept
        : handle_(handle) {}

    inline Promise& promise() const noexcept { return handle_.promise(); }

    inline void rethrow() const {
        if (const std::exception_ptr& error = handle_.promise().error)
            std::rethrow_exception(error);
    }

private:
    std::coroutine_handle<Promise> handle_;
};

}  // namespace task_detail

/// A coroutine somebody holds: it is handed back to its caller to be kept,
/// swept and read -- or awaited by another coroutine, which reads it for them.
///
/// That is the whole difference from `detached_task`, which owns itself and
/// answers to nobody. Here the caller keeps what comes back -- the Reader's
/// `Io` keeps them in a vector and sweeps the finished ones -- because there
/// is something to read at the end, and, while an operation of this module is
/// in flight, because there is something the worker still points at.
///
/// `task<T>` ends with a value, `task<>` without one; both end the same way.
/// `result()` gives the value or throws what left the coroutine, and gives the
/// value once, moved out. `co_await` inside another coroutine does the same
/// thing with the thread handed over instead of asked: the awaiting coroutine
/// suspends until this one ends and is resumed there, with the value or the
/// exception, on the thread this one ended on -- which is the one thread both
/// belong to. A task that has already ended is awaited without suspending.
///
/// It starts running the moment it is called (initial_suspend is
/// suspend_never), on the calling thread: an asynchronous operation is created
/// where the caller stands, and everything between two co_awaits runs there and
/// nowhere else. What each co_await does with the work in between -- send it to
/// another thread, or merely give the thread back to its own event loop -- is
/// the awaitable's business, not this one's. So what sets it apart from
/// `detached_task` is ownership, not where the work goes.
///
/// At the end the coroutine does not disappear (final_suspend suspends): the
/// frame is what the owner asks done() and takes the exception from, and the
/// only thing that happens there is the handover to a coroutine awaiting it.
/// The frame is destroyed by the task, and what that means while the coroutine
/// is still suspended depends on what it is suspended on.
///
/// **Dropping an unfinished one takes back whatever it waits for.**
/// Destroying the frame destroys its locals and the awaiter it stands in, and
/// nothing else happens: nobody is resumed, and no result is ever taken. A
/// task this one was awaiting lives among those locals and goes first, so a
/// chain is taken down from the inside out. For an awaitable that is one end
/// of a subscription on this same thread -- wxl.ui's event proxy, whose
/// awaiter unhooks in its destructor -- that is the ordinary way such a
/// coroutine is stopped, and the only one: an endless loop over an event has
/// no other end. For the asynchronous operations in this module the awaitable
/// gives its operation up (`async_op::abandon`), and the destruction waits, on
/// this thread, until the worker has let go of the frame -- as long as the
/// operation takes to finish or be cancelled.
///
/// **The frame comes from sta_memory_pool.** It is exactly what that pool is
/// for -- a small object, made and unmade on the one thread, over and over --
/// and it means the thread this coroutine belongs to has to be the pool's
/// thread, with the pool built before the first coroutine and outliving the
/// last. That is not a restriction this type adds: everything else in this
/// scheme is allocated there too, and a coroutine on any other thread would
/// have nowhere to put its operations anyway.
template <class T>
class task : public task_detail::owner<task_detail::value_promise<T>>
{
    using base = task_detail::owner<task_detail::value_promise<T>>;

public:
    using promise_type = task_detail::value_promise<T>;

    /// \return the value, moved out: ask once, after done().
    /// \throw whatever left the coroutine.
    inline T result() {
        this->rethrow();
        return std::move(*this->promise().result);
    }

    inline T await_resume() { return result(); }

private:
    friend promise_type;

    inline explicit task(std::coroutine_handle<promise_type> handle) noexcept
        : base(handle) {}
};

template <>
class task<void> : public task_detail::owner<task_detail::void_promise>
{
    using base = task_detail::owner<task_detail::void_promise>;

public:
    using promise_type = task_detail::void_promise;

    /// \throw whatever left the coroutine. Ask after done().
    inline void result() const { this->rethrow(); }

    inline void await_resume() const { result(); }

private:
    friend promise_type;

    inline explicit task(std::coroutine_handle<promise_type> handle) noexcept
        : base(handle) {}
};

template <class T>
inline task<T> task_detail::value_promise<T>::get_return_object() {
    return task<T>(std::coroutine_handle<value_promise>::from_promise(*this));
}

inline task<void> task_detail::void_promise::get_return_object() {
    return task<void>(std::coroutine_handle<void_promise>::from_promise(*this));
}

}  // export namespace wxl::async
