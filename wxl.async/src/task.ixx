module;

#include "coroutine_checks.h"

export module wxl.async:task;

import :async_op;
import :coroutine_checks;
import wxl.core;
import std;

export namespace wxl::async {

template <class R = void>
class task;

namespace task_detail {

using coro_detail::site;

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
/// because the compiler calls await_transform whenever the name is there at all; and a
/// link of the chain the promise derives through rather than a second base, so that it
/// takes no room in any compiler's layout.
template <class Base, bool = coro_detail::checked>
struct body_tracking : Base {};

template <class Base>
struct body_tracking<Base, true> : Base {
    template <class Awaitable>
    inline auto await_transform(Awaitable&& awaitable) {
        return tracked_awaiter<decltype(awaiter_of(std::forward<Awaitable>(awaitable)))>(
            this->continuation, std::forward<Awaitable>(awaitable));
    }
};

/// How the coroutine of a task gives its result back: with a value, or without one.
template <class R>
struct promise_returns : async_op_t<R> {
    /// By value rather than by forwarding reference: `co_return {}` and
    /// `co_return name_of_a_local` both have to work, and the move this costs an
    /// rvalue is nothing next to the operation the coroutine just awaited.
    inline void return_value(R value) { this->set_value(std::move(value)); }
};

template <>
struct promise_returns<void> : async_op_t<void> {
    inline void return_void() const noexcept {}
};

/// The promise of a task's coroutine: the producer of a result that is not carried out
/// elsewhere, so the co_await of a task reads it exactly as it reads an operation. The
/// frame comes from the pool (the allocation functions are async_op's), the body starts on
/// the calling thread, an exception is kept for whoever reads the task, and the end hands
/// the thread to the coroutine awaiting this one, if there is one.
template <class R>
class promise : public body_tracking<promise_returns<R>>
{
public:
    /// Set at birth: initial_suspend does not suspend, so the body starts on the stack.
    inline promise() noexcept { this->continuation.set_running(true); }

    task<R> get_return_object() noexcept;

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

        this->deliver_here();
        this->continuation.set_running(false);
        return handover{this->continuation.next()};
    }

    inline void unhandled_exception() noexcept { this->set_error(std::current_exception()); }

    /// The owner lets go: the frame is destroyed, its locals and the awaiter it stands in
    /// with it, and nothing else happens -- nobody is resumed, and no result is taken.
    inline void release() override {
        std::coroutine_handle<promise>::from_promise(*this).destroy();
    }

protected:
    /// Never called: a coroutine is not sent anywhere, its body runs where it is resumed.
    inline bool execute() override { return true; }
};

/// What a coroutine awaiting a task stands in: the producer of the task's result. It
/// parks the awaiting coroutine for whoever makes the result to wake -- the loop that
/// takes an operation back, the end of a coroutine -- and reads the result once the
/// thread comes back. The same few instructions whatever the producer is.
///
/// For the task a call has just returned this is the task itself, its public
/// base: the temporary lives in the awaiting frame and goes with it, so the
/// producer it already holds is all the wait needs and nothing has to be taken
/// back on the way out. A task kept elsewhere is awaited through a
/// join_awaiter instead.
///
/// The awaiting coroutine has to be on the task's own thread -- the thread of
/// the pool its producer came from, the one thread there is. A build that checks
/// makes sure.
///
/// Each hook is declared twice, and a build has one of the two: a strict build's
/// takes the place of the co_await as a defaulted parameter, which the compiler,
/// calling the hook, evaluates there; any other build's has no parameter, the
/// signature the hook always had, so that nothing new reaches the frame.
template <class R>
class awaiter
{
public:
    /// A task whose result is here already is awaited without suspending.
    inline bool await_ready() const noexcept requires(!coro_detail::strict) {
        return ready(site::current());
    }

    inline bool await_ready(site where = site::current()) const noexcept requires(coro_detail::strict) {
        return ready(where);
    }

    /// Nothing else happens: the producer is already at work, and wakes the waiter on
    /// this same thread.
    inline void await_suspend(std::coroutine_handle<> awaiting) noexcept requires(!coro_detail::strict) {
        park(awaiting, false, site::current());
    }

    inline void await_suspend(std::coroutine_handle<> awaiting, site where = site::current()) noexcept
        requires(coro_detail::strict)
    {
        park(awaiting, false, where);
    }

    inline decltype(auto) await_resume() requires(!coro_detail::strict) {
        return producer_->take_result(site::current());
    }

    inline decltype(auto) await_resume(site where = site::current()) requires(coro_detail::strict) {
        return producer_->take_result(where);
    }

protected:
    inline explicit awaiter(async_op_t<R>* producer) noexcept : producer_(producer) {}

    inline bool ready([[maybe_unused]] site where) const noexcept {
        coro_check(producer_, "task: moved-from", where);
        coro_check(core::sta_memory_pool::is_safe(), "task: awaited from a thread other than its own", where);
        return producer_->ready();
    }

    inline void park(std::coroutine_handle<> awaiting, bool by_join, [[maybe_unused]] site where) noexcept {
        coro_check(!producer_->continuation.waited(),
                   "task: awaited by a second coroutine while the first one still waits", where);
        producer_->continuation.park(awaiting, by_join);
    }

    async_op_t<R>* producer_;
};

/// The awaiter of a task that the awaiting coroutine does not own -- one kept
/// in a container or a field and joined there. Either of the two may go first,
/// so the awaiting frame takes its handle back out of the producer on its way out,
/// and a result made after its waiter is gone wakes nobody.
///
/// The task has to outlive the wait, as anything waited on does: one
/// destroyed while joined would leave this writing into its producer. A build
/// that checks marks the parked handle as a join's and catches that where the
/// task is destroyed.
template <class R>
class join_awaiter : public awaiter<R>
{
public:
    inline explicit join_awaiter(async_op_t<R>* producer) noexcept : awaiter<R>(producer) {}

    join_awaiter(const join_awaiter&) = delete;
    join_awaiter& operator=(const join_awaiter&) = delete;

    inline ~join_awaiter() { this->producer_->continuation.clear(); }

    inline void await_suspend(std::coroutine_handle<> awaiting) noexcept requires(!coro_detail::strict) {
        this->park(awaiting, true, site::current());
    }

    inline void await_suspend(std::coroutine_handle<> awaiting, site where = site::current()) noexcept
        requires(coro_detail::strict)
    {
        this->park(awaiting, true, where);
    }
};

}  // namespace task_detail

/// What a coroutine awaits, and the one thing it ever sees of asynchronous work: a
/// result in the making, handed back to its caller to be kept, swept and read -- or
/// awaited by another coroutine, which reads it for them. `file f = co_await
/// async_file::open_read(path);` and `co_await load(book)` are the same co_await.
///
/// Two kinds of producer stand behind it, and the awaiting coroutine cannot tell them
/// apart: an operation of this module (`async_op_t`), carried out on the worker, on the
/// system's thread pool or here, and woken back by the loop; and a coroutine returning a
/// task, which runs on this thread and hands the thread over at its end. Either way the
/// co_await asks whether the result is here, parks the waiter, and takes the result.
///
/// Who wakes the waiter is the producer's, and each says where: an operation's waiter is
/// resumed from inside the loop's call that takes the operation back (run_one(),
/// run_pending()); a coroutine's, from its own end -- the co_return, or the exception on
/// its way out -- by symmetric transfer, before the call that resumed the coroutine
/// returns. Nothing else resumes it, so the order is the program's own.
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
/// `result()` gives the value or throws what left the producer, and gives the
/// value once, moved out. `co_await` inside another coroutine does the same
/// thing with the thread handed over instead of asked: the awaiting coroutine
/// suspends until the result is here and is resumed there, with the value or the
/// exception, on the one thread both belong to. A task whose result is here
/// already is awaited without suspending.
///
/// The usual co_await is of the task a call has just returned, which lives and
/// dies with the awaiting frame. A task kept elsewhere is joined: `co_await`
/// of an lvalue, from any coroutine of the thread, one at a time; the waiter
/// that goes first takes itself off. The task has to outlive the wait.
///
/// It starts the moment it is made, either kind, on the calling thread: an operation
/// is sent inside the call that starts it, and a coroutine runs until its first
/// suspension (initial_suspend is suspend_never), so `auto a = f.read(x); auto b =
/// g.read(y); co_await a; co_await b;` has both under way before either is waited on.
/// What each co_await does with the work in between -- send it to another thread, or
/// merely give the thread back to its own event loop -- is the producer's business.
///
/// **Dropping an unfinished one takes back whatever it waits for.** An operation
/// still out is given up (`async_op::abandon`): it is asked to cancel, and the
/// destruction waits, on this thread, until the worker has let go of it -- as long as
/// the operation takes to finish or be cancelled -- unless it is orphanable and may
/// finish alone. A coroutine's frame is destroyed, its locals and the awaiter it stands
/// in with it, and nothing else happens: nobody is resumed, and no result is ever
/// taken. A task it was awaiting lives among those locals and goes first, so a chain is
/// taken down from the inside out. For a wait on an event of this thread -- wxl.ui's
/// event proxy, whose awaiter unhooks in its destructor -- that is the ordinary way such
/// a coroutine is stopped, and the only one: an endless loop over an event has no other
/// end. It is dropped from outside the chain: a frame that is on the stack cannot be
/// destroyed under it, and a Debug build catches the attempt -- any build does under
/// STRICT_CORO.
///
/// **Its producer comes from sta_memory_pool**, an operation and a coroutine's frame
/// alike. It is exactly what that pool is for -- a small object, made and unmade on the
/// one thread, over and over -- and it means the thread a task belongs to has to be the
/// pool's thread, with the pool built before the first and outliving the last.
template <class R>
class [[nodiscard("a task nobody keeps is destroyed at once, and its work with it")]] task
    : public task_detail::awaiter<R>
{
public:
    using promise_type = task_detail::promise<R>;

    /// Takes over an operation already started: what the calls of sta_loop do with the
    /// operations they start, and what an author of an operation of their own does in the
    /// call that starts it.
    inline explicit task(std::unique_ptr<async_op_t<R>> started) noexcept
        : task_detail::awaiter<R>(started.release()) {}

    inline task(task&& other) noexcept
        : task_detail::awaiter<R>(std::exchange(other.producer_, nullptr)) {}

    inline task& operator=(task&& other) noexcept {
        std::swap(this->producer_, other.producer_);
        return *this;
    }

    /// A coroutine's frame is destroyed, its locals with it -- a task it awaits among
    /// them, so a chain is taken down from the inside out; an operation is deleted, or
    /// given up if it is still out. Where a destructor is called nothing gives the line,
    /// so a check here names its own.
    inline ~task() {
        if (this->producer_) {
            coro_check(!this->producer_->continuation.running(),
                       "task: destroyed while its coroutine runs -- dropped from inside its own chain",
                       task_detail::site::current());
            coro_check(!this->producer_->continuation.joined(), "task: destroyed while a coroutine joins it",
                       task_detail::site::current());

            this->producer_->release();
        }
    }

    /// \return `true` once the result is here: the coroutine has run to its end, by
    ///         reaching it or by leaving through an exception; the loop has taken the
    ///         operation back.
    inline bool done() const noexcept { return this->producer_->ready(); }

    /// \return the value, moved out: ask once, after done().
    /// \throw whatever left the producer.
    /// \param where the caller's line, for a strict build's report; left to its default.
    inline R result(task_detail::site where = task_detail::site::current()) requires(!std::is_void_v<R>) {
        check_ended(where);
        return this->producer_->take_result(where);
    }

    /// \throw whatever left the producer. Ask after done(), as often as needed.
    /// \param where the caller's line, for a strict build's report; left to its default.
    inline void result(task_detail::site where = task_detail::site::current()) const
        requires(std::is_void_v<R>)
    {
        check_ended(where);
        this->producer_->take_result(where);
    }

    /// A task awaited as an lvalue is joined: it belongs to somebody else, and
    /// the wait gets an awaiter of its own in the awaiting frame. Nothing here
    /// matches an rvalue, so co_await takes that one as it stands -- its own
    /// awaiter, with not a word added to the frame. A const one is not awaited:
    /// waiting writes into it. The last condition keeps out whatever else
    /// argument-dependent lookup brings here, such as an optional of a task.
    template <class Self>
        requires(std::is_lvalue_reference_v<Self> && !std::is_const_v<std::remove_reference_t<Self>> &&
                 std::derived_from<std::remove_reference_t<Self>, task>)
    inline friend task_detail::join_awaiter<R> operator co_await(Self&& kept) noexcept {
        return task_detail::join_awaiter<R>(kept.producer_);
    }

private:
    friend promise_type;

    inline explicit task(async_op_t<R>* producer) noexcept : task_detail::awaiter<R>(producer) {}

    inline void check_ended([[maybe_unused]] task_detail::site where) const noexcept {
        coro_check(this->producer_, "task: moved-from", where);
        coro_check(this->producer_->ready(), "task: result() asked before it ended", where);
    }
};

template <class R>
inline task<R> task_detail::promise<R>::get_return_object() noexcept {
    return task<R>(this);
}

}  // export namespace wxl::async
