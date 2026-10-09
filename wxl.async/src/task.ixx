module;

#include "coroutine_checks.h"

export module wxl.async:task;

import :coroutine_checks;
import wxl.core;
import std;

export namespace wxl::async {

template <class T = void>
class task;

namespace task_detail {

using coro_detail::site;

/// The coroutine awaiting a task, in one word. The handle's address is that of a frame,
/// which starts with pointers, so its two lowest bits are free; where the checks are
/// compiled in they say whether the waiter stands in a join_awaiter and whether the
/// task's own body is on the stack. Without the checks the bits are never set, and the
/// word is the handle and nothing else -- kept as a pointer, as the handle keeps it, so
/// that the compiler treats it the same.
class continuation_word
{
public:
    /// Whom final_suspend hands the thread to: the waiter, or nobody.
    inline std::coroutine_handle<> next() const noexcept {
        const std::uintptr_t handle = bits() & ~flags;
        return handle ? std::coroutine_handle<>::from_address(reinterpret_cast<void*>(handle))
                      : std::noop_coroutine();
    }

    inline bool waited() const noexcept { return (bits() & ~flags) != 0; }
    inline bool joined() const noexcept { return (bits() & joined_bit) != 0; }
    inline bool running() const noexcept { return (bits() & running_bit) != 0; }

    /// The waiter parks; the body's own bit stays as it is.
    inline void park(std::coroutine_handle<> waiter, bool by_join) noexcept {
        set((bits() & running_bit) | reinterpret_cast<std::uintptr_t>(waiter.address()) |
            (by_join ? joined_bit : 0));
    }

    /// The waiter leaves; the body's own bit stays as it is.
    inline void clear() noexcept { set(bits() & running_bit); }

    inline void set_running(bool running) noexcept {
        if constexpr (coro_detail::checked) set(running ? bits() | running_bit : bits() & ~running_bit);
    }

private:
    static constexpr std::uintptr_t joined_bit = coro_detail::checked ? 1 : 0;
    static constexpr std::uintptr_t running_bit = coro_detail::checked ? 2 : 0;
    static constexpr std::uintptr_t flags = joined_bit | running_bit;

    inline std::uintptr_t bits() const noexcept { return reinterpret_cast<std::uintptr_t>(word_); }
    inline void set(std::uintptr_t bits) noexcept { word_ = reinterpret_cast<void*>(bits); }

    /// Set at birth: initial_suspend does not suspend, so the body starts on the stack.
    void* word_ = reinterpret_cast<void*>(running_bit);
};

/// The awaiter co_await would use for an awaitable: what a member operator
/// co_await returns, else what a free one returns, else the awaitable itself,
/// by reference -- it outlives the co_await either way.
template <class Awaitable>
inline decltype(auto) awaiter_of(Awaitable&& awaitable) {
    if constexpr (requires { std::forward<Awaitable>(awaitable).operator co_await(); })
        return std::forward<Awaitable>(awaitable).operator co_await();
    else if constexpr (requires { operator co_await(std::forward<Awaitable>(awaitable)); })
        return operator co_await(std::forward<Awaitable>(awaitable));
    else
        return static_cast<std::remove_reference_t<Awaitable>&>(awaitable);
}

/// A co_await in the body of a task, as a build that checks compiles it: the same
/// awaiter, with the body's bit taken down while the body is suspended, so that
/// destroying a frame that is on the stack is caught. Each hook takes the place of the
/// co_await -- the compiler calls it, so the default is evaluated there -- and hands it
/// on to an awaiter that takes one.
template <class Awaiter>
class tracked_awaiter
{
public:
    template <class Awaitable>
    inline tracked_awaiter(continuation_word& continuation, Awaitable&& awaitable)
        : continuation_(continuation), awaiter_(awaiter_of(std::forward<Awaitable>(awaitable))) {}

    inline bool await_ready(site where = site::current()) {
        if constexpr (requires { awaiter_.await_ready(where); })
            return awaiter_.await_ready(where);
        else
            return awaiter_.await_ready();
    }

    /// Down before the hand-over: past it, the frame may already have been
    /// resumed elsewhere, or destroyed.
    template <class Promise>
    inline decltype(auto) await_suspend(std::coroutine_handle<Promise> self, site where = site::current()) {
        continuation_.set_running(false);

        if constexpr (requires { awaiter_.await_suspend(self, where); }) {
            // One of this module's, which do not throw.
            return awaiter_.await_suspend(self, where);
        } else if constexpr (noexcept(awaiter_.await_suspend(self))) {
            return awaiter_.await_suspend(self);
        } else {
            // An await_suspend that throws resumes the coroutine with the exception.
            try {
                return awaiter_.await_suspend(self);
            } catch (...) {
                continuation_.set_running(true);
                throw;
            }
        }
    }

    inline decltype(auto) await_resume(site where = site::current()) {
        continuation_.set_running(true);

        if constexpr (requires { awaiter_.await_resume(where); })
            return awaiter_.await_resume(where);
        else
            return awaiter_.await_resume();
    }

private:
    continuation_word& continuation_;
    Awaiter awaiter_;
};

/// What only a build that checks gives a task's promise: await_transform, through
/// which every co_await of the body passes. It is a base, not a constrained member,
/// because the compiler calls await_transform whenever the name is there at all.
template <class Promise, bool = coro_detail::checked>
struct body_tracking {};

template <class Promise>
struct body_tracking<Promise, true> {
    template <class Awaitable>
    inline auto await_transform(Awaitable&& awaitable) {
        return tracked_awaiter<decltype(awaiter_of(std::forward<Awaitable>(awaitable)))>(
            static_cast<Promise&>(*this).continuation, std::forward<Awaitable>(awaitable));
    }
};

/// What every task's promise has in common: the frame from the pool, the
/// start on the calling thread, the exception kept for whoever reads the
/// task, and the way out -- which hands the thread to the coroutine awaiting
/// this one, if there is one.
struct promise_base : body_tracking<promise_base> {
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
        struct handover {
            std::coroutine_handle<> next;

            inline bool await_ready() const noexcept { return false; }
            inline std::coroutine_handle<> await_suspend(std::coroutine_handle<>) const noexcept {
                return next;
            }
            inline void await_resume() const noexcept {}
        };

        continuation.set_running(false);
        return handover{continuation.next()};
    }

    inline void unhandled_exception() noexcept { error = std::current_exception(); }

    inline void rethrow() const {
        if (error) std::rethrow_exception(error);
    }

    /// The coroutine awaiting this one, parked by the awaiter's await_suspend;
    /// empty while nobody is. One at most: a task is awaited once at a time.
    continuation_word continuation;
    std::exception_ptr error;
};

template <class T>
struct value_promise : promise_base {
    task<T> get_return_object();

    /// By value rather than by forwarding reference: `co_return {}` and
    /// `co_return name_of_a_local` both have to work, and the move this costs an
    /// rvalue is nothing next to the operation the coroutine just awaited.
    inline void return_value(T value) { result.emplace(std::move(value)); }

    /// The value, moved out, or what left the coroutine. The value goes once:
    /// a build that checks empties the optional behind it, so a second read fails
    /// the check instead of handing over a moved-from value. The exception stays.
    inline T take_result([[maybe_unused]] site where) {
        rethrow();
        coro_check(result.has_value(), "task: the value has already been taken", where);

        if constexpr (coro_detail::checked) {
            T taken = std::move(*result);
            result.reset();
            return taken;
        } else {
            return std::move(*result);
        }
    }

    std::optional<T> result;
};

struct void_promise : promise_base {
    task<void> get_return_object();

    inline void return_void() const noexcept {}

    /// Nothing is moved out, so asking again is asking again: what left the
    /// coroutine is thrown each time.
    inline void take_result(site) const { rethrow(); }
};

/// What a coroutine awaiting a task stands in: the task's handle. It parks
/// the awaiting coroutine for final_suspend to hand the thread to, and reads
/// the result once the thread comes back.
///
/// For the task a call has just returned this is the task itself, its public
/// base: the temporary lives in the awaiting frame and goes with it, so the
/// handle it already holds is all the wait needs and nothing has to be taken
/// back on the way out. A task kept elsewhere is awaited through a
/// join_awaiter instead.
///
/// The awaiting coroutine has to be on the task's own thread -- the thread of
/// the pool its frame came from, the one thread there is. A build that checks
/// makes sure.
///
/// Each hook is declared twice, and a build has one of the two: a strict build's
/// takes the place of the co_await as a defaulted parameter, which the compiler,
/// calling the hook, evaluates there; any other build's has no parameter, the
/// signature the hook always had, so that nothing new reaches the frame.
template <class Promise>
class awaiter
{
public:
    /// A task that has already ended is awaited without suspending.
    inline bool await_ready() const noexcept requires(!coro_detail::strict) {
        return ready(site::current());
    }

    inline bool await_ready(site where = site::current()) const noexcept requires(coro_detail::strict) {
        return ready(where);
    }

    /// Nothing else happens: the task is already running, or suspended on an
    /// operation that will resume it on this same thread.
    inline void await_suspend(std::coroutine_handle<> awaiting) noexcept requires(!coro_detail::strict) {
        park(awaiting, false, site::current());
    }

    inline void await_suspend(std::coroutine_handle<> awaiting, site where = site::current()) noexcept
        requires(coro_detail::strict)
    {
        park(awaiting, false, where);
    }

    inline decltype(auto) await_resume() requires(!coro_detail::strict) {
        return handle_.promise().take_result(site::current());
    }

    inline decltype(auto) await_resume(site where = site::current()) requires(coro_detail::strict) {
        return handle_.promise().take_result(where);
    }

protected:
    inline explicit awaiter(std::coroutine_handle<Promise> handle) noexcept : handle_(handle) {}

    inline bool ready([[maybe_unused]] site where) const noexcept {
        coro_check(core::sta_memory_pool::is_safe(), "task: awaited from a thread other than its own", where);
        return handle_.done();
    }

    inline void park(std::coroutine_handle<> awaiting, bool by_join, [[maybe_unused]] site where) noexcept {
        coro_check(!handle_.promise().continuation.waited(),
                   "task: awaited by a second coroutine while the first one still waits", where);
        handle_.promise().continuation.park(awaiting, by_join);
    }

    std::coroutine_handle<Promise> handle_;
};

/// The awaiter of a task that the awaiting coroutine does not own -- one kept
/// in a container or a field and joined there. Either of the two may go first,
/// so the awaiting frame takes its handle back out of the task on its way out,
/// and a task that ends after its waiter is gone hands the thread to nobody.
///
/// The task has to outlive the wait, as anything waited on does: one
/// destroyed while joined would leave this writing into its frame. A build
/// that checks marks the parked handle as a join's and catches that where the
/// task is destroyed.
template <class Promise>
class join_awaiter : public awaiter<Promise>
{
public:
    inline explicit join_awaiter(std::coroutine_handle<Promise> handle) noexcept
        : awaiter<Promise>(handle) {}

    join_awaiter(const join_awaiter&) = delete;
    join_awaiter& operator=(const join_awaiter&) = delete;

    inline ~join_awaiter() { this->handle_.promise().continuation.clear(); }

    inline void await_suspend(std::coroutine_handle<> awaiting) noexcept requires(!coro_detail::strict) {
        this->park(awaiting, true, site::current());
    }

    inline void await_suspend(std::coroutine_handle<> awaiting, site where = site::current()) noexcept
        requires(coro_detail::strict)
    {
        this->park(awaiting, true, where);
    }
};

/// The handle and what every task does with it: owns the frame, answers
/// done(), and lets another coroutine await it.
template <class Promise>
class owner : public awaiter<Promise>
{
public:
    inline owner(owner&& other) noexcept
        : awaiter<Promise>(std::exchange(other.handle_, {})) {}

    inline owner& operator=(owner&& other) noexcept {
        std::swap(this->handle_, other.handle_);
        return *this;
    }

    /// Where a destructor is called nothing gives the line, so a check here names
    /// its own.
    inline ~owner() {
        if (this->handle_) {
            coro_check(!promise().continuation.running(),
                       "task: destroyed while its coroutine runs -- dropped from inside its own chain",
                       site::current());
            coro_check(!promise().continuation.joined(), "task: destroyed while a coroutine joins it",
                       site::current());
            this->handle_.destroy();
        }
    }

    /// \return `true` once the coroutine has run to its end, whether by
    ///         reaching it or by leaving through an exception.
    inline bool done() const noexcept { return this->handle_.done(); }

    /// A task awaited as an lvalue is joined: it belongs to somebody else, and
    /// the wait gets an awaiter of its own in the awaiting frame. Nothing here
    /// matches an rvalue, so co_await takes that one as it stands -- its own
    /// awaiter, with not a word added to the frame. A const one is not awaited:
    /// waiting writes into it. The last condition keeps out whatever else
    /// argument-dependent lookup brings here, such as an optional of a task.
    template <class Self>
        requires(std::is_lvalue_reference_v<Self> && !std::is_const_v<std::remove_reference_t<Self>> &&
                 std::derived_from<std::remove_reference_t<Self>, owner>)
    inline friend join_awaiter<Promise> operator co_await(Self&& kept) noexcept {
        return join_awaiter<Promise>(kept.handle_);
    }

protected:
    inline explicit owner(std::coroutine_handle<Promise> handle) noexcept
        : awaiter<Promise>(handle) {}

    inline Promise& promise() const noexcept { return this->handle_.promise(); }
};

}  // namespace task_detail

/// A coroutine somebody holds: it is handed back to its caller to be kept,
/// swept and read -- or awaited by another coroutine, which reads it for them.
///
/// That is the whole difference from `detached_task`, which owns itself and
/// answers to nobody. Here the caller keeps what comes back -- the Reader's
/// `Io` keeps them in a vector and sweeps the finished ones -- because there
/// is something to read at the end, and, while an operation of this module is
/// in flight, because there is something the worker still points at. Not
/// keeping it is dropping it, which takes its work down at once; the compiler
/// says so.
///
/// `task<T>` ends with a value, `task<>` without one; both end the same way.
/// `result()` gives the value or throws what left the coroutine, and gives the
/// value once, moved out. `co_await` inside another coroutine does the same
/// thing with the thread handed over instead of asked: the awaiting coroutine
/// suspends until this one ends and is resumed there, with the value or the
/// exception, on the thread this one ended on -- which is the one thread both
/// belong to. A task that has already ended is awaited without suspending.
///
/// The usual co_await is of the task a call has just returned, which lives and
/// dies with the awaiting frame. A task kept elsewhere is joined: `co_await`
/// of an lvalue, from any coroutine of the thread, one at a time; the waiter
/// that goes first takes itself off. The task has to outlive the wait.
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
/// operation takes to finish or be cancelled. It is dropped from outside the
/// chain: a frame that is on the stack cannot be destroyed under it, and a
/// Debug build catches the attempt -- any build does under STRICT_CORO.
///
/// **The frame comes from sta_memory_pool.** It is exactly what that pool is
/// for -- a small object, made and unmade on the one thread, over and over --
/// and it means the thread this coroutine belongs to has to be the pool's
/// thread, with the pool built before the first coroutine and outliving the
/// last. That is not a restriction this type adds: everything else in this
/// scheme is allocated there too, and a coroutine on any other thread would
/// have nowhere to put its operations anyway.
template <class T>
class [[nodiscard("a task nobody keeps is destroyed at once, and its work with it")]] task
    : public task_detail::owner<task_detail::value_promise<T>>
{
    using base = task_detail::owner<task_detail::value_promise<T>>;

public:
    using promise_type = task_detail::value_promise<T>;

    /// \return the value, moved out: ask once, after done().
    /// \throw whatever left the coroutine.
    /// \param where the caller's line, for a strict build's report; left to its default.
    inline T result(task_detail::site where = task_detail::site::current()) {
        coro_check(this->done(), "task: result() asked before the coroutine ended", where);
        return this->promise().take_result(where);
    }

private:
    friend promise_type;

    inline explicit task(std::coroutine_handle<promise_type> handle) noexcept
        : base(handle) {}
};

template <>
class [[nodiscard("a task nobody keeps is destroyed at once, and its work with it")]] task<void>
    : public task_detail::owner<task_detail::void_promise>
{
    using base = task_detail::owner<task_detail::void_promise>;

public:
    using promise_type = task_detail::void_promise;

    /// \throw whatever left the coroutine. Ask after done(), as often as needed.
    /// \param where the caller's line, for a strict build's report; left to its default.
    inline void result(task_detail::site where = task_detail::site::current()) const {
        coro_check(this->done(), "task: result() asked before the coroutine ended", where);
        this->promise().take_result(where);
    }

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
