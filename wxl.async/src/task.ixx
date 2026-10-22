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

namespace detail {

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

/// The frames of coroutines returning a task that exist: made and not yet unmade, running
/// or suspended. A plain number, since the frames live on the one STA thread, and counted
/// only by a build that checks: elsewhere the frame's allocation does nothing more.
struct task_frames {
    static inline std::size_t live = 0;
};

/// The producer of a task's coroutine: the result its body makes, the waiter, and the
/// frame while the body is in flight. It is made with the frame and outlives it -- the body
/// ends, its frame goes there and then, and the result stays here for the task to read --
/// so a task's co_await reads a coroutine exactly as it reads an operation. From the pool,
/// like every producer (async_op's allocation functions).
template <class R>
class coroutine_op final : public async_op_t<R>
{
public:
    inline explicit coroutine_op(std::coroutine_handle<> frame) noexcept : frame_(frame) {}

    /// The value of the body's co_return.
    template <class Value>
    inline void keep_value(Value&& value) {
        this->set_value(std::forward<Value>(value));
    }

    /// What left the body: the exception being handled. A body that has made its value
    /// already ended with co_return, and what threw after it is the destructor of a local:
    /// a broken rule, which a build that checks stops at rather than keep both answers.
    inline void keep_current_exception() noexcept {
        coro_check(!this->value_made(), "task: a destructor threw after co_return", detail::site::current());
        this->set_current_exception();
    }

    /// The body is over, its locals gone: the result is here, and the frame goes next.
    inline void end_body() noexcept {
        this->continuation.set_running(false);
        this->complete_in_place();
    }

    /// The owner lets go. A body that has ended has taken its frame with it, and the result
    /// goes; a body still in flight is stop_running_body()'s.
    inline void release() override {
        if (this->ready())
            delete this;
        else
            stop_running_body();
    }

protected:
    /// Never called: a coroutine is not sent anywhere, its body runs where it is resumed.
    inline bool execute() override { return true; }

private:
    /// What becomes of a body in flight whose task goes: the one place that decides it, and
    /// the point two designs of it part at. This one destroys the frame where it stands
    /// suspended -- its locals go, a task or an operation it awaits among them, and nobody is
    /// resumed -- and the producer after it, with no result ever made.
    inline void stop_running_body() {
        frame_.destroy();
        delete this;
    }

    /// Valid while the body is in flight; the body's end takes the frame with it.
    std::coroutine_handle<> frame_;
};

/// What only a build that checks gives a task's promise: await_transform, through
/// which every co_await of the body passes. It is a base, not a constrained member,
/// because the compiler calls await_transform whenever the name is there at all; and a
/// link of the chain the promise derives through rather than a second base, so that it
/// takes no room in any compiler's layout.
template <class Base, bool = detail::checked>
struct body_tracking : Base {};

template <class Base>
struct body_tracking<Base, true> : Base {
    template <class Awaitable>
    inline auto await_transform(Awaitable&& awaitable) {
        return tracked_awaiter<decltype(awaiter_of(std::forward<Awaitable>(awaitable)))>(
            this->producer->continuation, std::forward<Awaitable>(awaitable));
    }
};

/// How the coroutine of a task gives its result back: with a value, or without one -- to its
/// producer, the one thing the promise holds.
template <class R>
struct promise_returns {
    /// By value rather than by forwarding reference: `co_return {}` and
    /// `co_return name_of_a_local` both have to work, and the move this costs an
    /// rvalue is nothing next to the operation the coroutine just awaited.
    inline void return_value(R value) { producer->keep_value(std::move(value)); }

    coroutine_op<R>* producer = nullptr;
};

template <>
struct promise_returns<void> {
    inline void return_void() const noexcept {}

    coroutine_op<void>* producer = nullptr;
};

/// The promise of a task's coroutine. It holds a pointer to the producer of the result and
/// nothing else: the result, the waiter and whether it has ended are the producer's, made in
/// get_return_object and outliving the frame. The frame comes from the pool, the body
/// starts on the calling thread, an exception is kept for whoever reads the task, and the
/// end takes the frame down and hands the thread to the coroutine awaiting this one, if
/// there is one.
template <class R>
class task_promise : public body_tracking<promise_returns<R>>
{
public:
    /// The frame, from the pool: a small object made and unmade on the one thread, over
    /// and over. The pool asserts the thread in debug builds, so a frame made anywhere else
    /// is caught here. Counted by a build that checks: a frame is alive from here to operator
    /// delete.
    inline static void* operator new(std::size_t size) {
        void* const frame = core::sta_memory_pool::alloc(size);
        if constexpr (detail::checked) ++task_frames::live;
        return frame;
    }

    inline static void operator delete(void* frame, std::size_t size) noexcept {
        if constexpr (detail::checked) --task_frames::live;
        core::sta_memory_pool::free(frame, size);
    }

    /// Makes the producer -- the second allocation of a call, from the same pool -- which
    /// the task returned holds. The body is on the stack from here: initial_suspend does
    /// not suspend.
    task<R> get_return_object();

    inline std::suspend_never initial_suspend() const noexcept { return {}; }

    /// The body is over and its locals are gone; what is left of the frame -- the promise,
    /// the copies of the parameters, the memory -- goes now, before anybody reads the
    /// result, which is in the producer. With nobody awaiting, the end does not suspend,
    /// and the frame goes as the coroutine flows off it; the thread goes back to whoever
    /// resumed it. With a waiter, the end suspends, destroys its own frame and hands the
    /// thread to the waiter by symmetric transfer, so that a chain of awaiting coroutines
    /// finishes one after another without the stack growing by a frame per link; the
    /// waiter is read out before the frame goes, and nothing of the frame -- this awaiter
    /// lives in it -- is touched after.
    inline auto final_suspend() noexcept {
        struct handover {
            coroutine_op<R>* producer;

            inline bool await_ready() const noexcept { return !producer->continuation.waited(); }

            inline std::coroutine_handle<> await_suspend(std::coroutine_handle<> self) const noexcept {
                const std::coroutine_handle<> next = producer->continuation.next();
                self.destroy();
                return next;
            }

            inline void await_resume() const noexcept {}
        };

        this->producer->end_body();
        return handover{this->producer};
    }

    inline void unhandled_exception() noexcept { this->producer->keep_current_exception(); }
};

/// Whether a promise is that of a coroutine returning a task -- the one kind of coroutine
/// a task may be awaited from.
template <class Promise>
inline constexpr bool is_task_promise = false;

template <class R>
inline constexpr bool is_task_promise<task_promise<R>> = true;

/// Who may await a task: a coroutine returning a task, and nothing else. Its frame comes
/// from the pool on the pool's thread, its co_await passes through the checks of its
/// promise, and it is stopped by dropping its own task -- the rules a wait on a task leans
/// on. Any other coroutine, behind an erased std::coroutine_handle<> or a promise of its
/// own, keeps none of them, so a co_await of a task from it does not compile: the
/// awaiter's await_suspend takes only a handle of this kind.
template <class Promise>
concept task_coroutine_promise = is_task_promise<Promise>;

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
/// The awaiting coroutine is a coroutine returning a task (task_coroutine_promise): the
/// compiler refuses any other. It has to be on the task's own thread -- the thread of
/// the pool its producer came from, the one thread there is; a body moved off it by an
/// awaitable of somebody else's is caught by a build that checks.
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
    inline bool await_ready() const noexcept requires(!detail::strict) {
        return ready(site::current());
    }

    inline bool await_ready(site where = site::current()) const noexcept requires(detail::strict) {
        return ready(where);
    }

    /// Nothing else happens: the producer is already at work, and wakes the waiter on
    /// this same thread.
    template <class Promise>
        requires(task_coroutine_promise<Promise> && !detail::strict)
    inline void await_suspend(std::coroutine_handle<Promise> awaiting) noexcept {
        park(awaiting, false, site::current());
    }

    template <class Promise>
        requires(task_coroutine_promise<Promise> && detail::strict)
    inline void await_suspend(std::coroutine_handle<Promise> awaiting, site where = site::current()) noexcept {
        park(awaiting, false, where);
    }

    inline decltype(auto) await_resume() requires(!detail::strict) {
        return producer_->take_result(site::current());
    }

    inline decltype(auto) await_resume(site where = site::current()) requires(detail::strict) {
        return producer_->take_result(where);
    }

protected:
    inline explicit awaiter(async_op_t<R>* producer) noexcept : producer_(producer) {}

    inline bool ready([[maybe_unused]] site where) const noexcept {
        coro_check(producer_, "task: empty -- default-constructed or moved-from", where);
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
/// task is destroyed. Whoever waits through an object holds the object: the
/// frame of the coroutine joined goes, copies of its parameters with it, before
/// the waiter is resumed, so an object only that coroutine holds goes then, and
/// a task in its field with it.
template <class R>
class join_awaiter : public awaiter<R>
{
public:
    inline explicit join_awaiter(async_op_t<R>* producer) noexcept : awaiter<R>(producer) {}

    join_awaiter(const join_awaiter&) = delete;
    join_awaiter& operator=(const join_awaiter&) = delete;

    inline ~join_awaiter() { this->producer_->continuation.clear(); }

    template <class Promise>
        requires(task_coroutine_promise<Promise> && !detail::strict)
    inline void await_suspend(std::coroutine_handle<Promise> awaiting) noexcept {
        this->park(awaiting, true, site::current());
    }

    template <class Promise>
        requires(task_coroutine_promise<Promise> && detail::strict)
    inline void await_suspend(std::coroutine_handle<Promise> awaiting, site where = site::current()) noexcept {
        this->park(awaiting, true, where);
    }
};

}  // namespace detail

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
/// A coroutine's frame lasts as long as its body and no longer: at the end, the copies of
/// its parameters and its memory go with it -- before the waiter is resumed, or right
/// there when nobody waits -- and the result stays behind in its producer, which the task
/// holds, to be read whenever its owner asks.
///
/// Who wakes the waiter is the producer's, and each says where: an operation's waiter is
/// resumed from inside the loop's call that takes the operation back (run_one(),
/// run_pending()) -- or, for wxl.ui's operation of the platform, from the call the thread's
/// dispatcher queue makes; a coroutine's, from its own end -- the co_return, or the exception on
/// its way out -- by symmetric transfer, before the call that resumed the coroutine
/// returns. Nothing else resumes it, so the order is the program's own.
///
/// **A task lives in a named place**: a local, or a field of whoever knows when
/// to stop it -- a window its load, a model its save; one that runs several keeps
/// them in a vector and sweeps the finished ones. Dropping a task whose body runs
/// stops it, and there is no fire-and-forget: the compiler refuses a task nobody
/// keeps. What it ends with is there to be read, and a failure nobody reads is
/// nothing more than that, as with std::future. Whoever keeps a task stops it by
/// its token, or drops it, before the end of the process: the end waits for the
/// operations still out and not for bodies, and a build that checks stops at a
/// frame still alive there (live_frames()).
///
/// `task<T>` ends with a value, `task<>` without one; both end the same way.
/// `result()` gives the value or throws what left the producer, and gives the
/// value once, moved out. `has_value()`, `has_exception()` and
/// `get_exception_ptr()` say how it ended without throwing, as a future's do;
/// there is no wait() -- a task is waited for by co_await, or turned to its end by
/// sta_loop::run_until(). `co_await` inside another coroutine does the same
/// thing with the thread handed over instead of asked: the awaiting coroutine
/// suspends until the result is here and is resumed there, with the value or the
/// exception, on the one thread both belong to. A task whose result is here
/// already is awaited without suspending.
///
/// Only a coroutine that itself returns a task awaits one: a co_await of a task from
/// a coroutine of any other kind does not compile (task_coroutine_promise). Outside
/// a coroutine a task is read by done() and result(), or turned to its end by
/// sta_loop::run_until().
///
/// The usual co_await is of the task a call has just returned, which lives and
/// dies with the awaiting frame. A task kept elsewhere is joined: `co_await`
/// of an lvalue, from any task coroutine of the thread, one at a time; the waiter
/// that goes first takes itself off. The task has to outlive the wait -- and
/// one in a field of an object, the object, held by the waiter rather than
/// only by the coroutine it waits on.
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
/// finish alone. A coroutine still in flight has its frame destroyed, its locals and the
/// awaiter it stands in with it, and nothing else happens: nobody is resumed, and no
/// result is ever made. A task it was awaiting lives among those locals and goes first, so a chain is
/// taken down from the inside out. For a wait on an event of this thread -- wxl.ui's
/// event proxy, whose awaiter unhooks in its destructor -- that is the ordinary way such
/// a coroutine is stopped, and the only one: an endless loop over an event has no other
/// end. It is dropped from outside the chain: a frame that is on the stack cannot be
/// destroyed under it, and a Debug build catches the attempt -- any build does under
/// STRICT_CORO.
///
/// **An empty task holds nothing**: one made by the default constructor, and one moved
/// from -- `std::exchange(field, {})` takes the task out of a field and leaves it empty. It
/// waits for nothing and gives nothing: `bool(t)` says whether there is a task in it,
/// destroying it or assigning over it does nothing more, and anything else asked of it --
/// co_await, done(), result() -- is a broken rule, which a build that checks stops at. That
/// is the state of a field that holds a task at one time and not at another, a load under
/// way or none: the field is a task, and an optional around it would be a second empty
/// state over the one the task has.
///
/// **Its producer comes from sta_memory_pool**, an operation and a coroutine's alike, and
/// so does a coroutine's frame. It is exactly what that pool is for -- a small object, made and unmade on the
/// one thread, over and over -- and it means the thread a task belongs to has to be the
/// pool's thread, with the pool built before the first and outliving the last.
template <class R>
class [[nodiscard("a task nobody keeps is destroyed at once, and its work with it")]] task
    : public detail::awaiter<R>
{
public:
    using promise_type = detail::task_promise<R>;

    /// An empty task: nothing to wait for, nothing to give.
    inline task() noexcept : detail::awaiter<R>(nullptr) {}

    /// Takes over an operation already started: what the calls of sta_loop do with the
    /// operations they start, and what an author of an operation of their own does in the
    /// call that starts it.
    inline explicit task(std::unique_ptr<async_op_t<R>> started) noexcept
        : detail::awaiter<R>(started.release()) {}

    inline task(task&& other) noexcept
        : detail::awaiter<R>(std::exchange(other.producer_, nullptr)) {}

    /// Takes over what `other` holds, and leaves it empty. The task this one held is dropped
    /// here, the way its destructor drops it -- an operation still out given up, a
    /// coroutine in flight stopped -- rather than handed to `other` to go whenever that one
    /// does: a task moved from is empty, whichever way it was moved. Through a local, so
    /// that a task moved onto itself stays as it is.
    ///
    /// Assign over a task only while its body is suspended or over: a body may not replace
    /// the task that holds it while it runs -- from its own code or from a call it makes --
    /// since that would destroy the frame it runs in. A build that checks stops there.
    inline task& operator=(task&& other) noexcept {
        task taken(std::move(other));
        std::swap(this->producer_, taken.producer_);
        return *this;
    }

    /// The result goes. A coroutine still in flight has its frame destroyed first, its locals
    /// with it -- a task it awaits among them, so a chain is taken down from the inside out;
    /// an operation still out is given up. Where a destructor is called nothing gives the line,
    /// so a check here names its own.
    inline ~task() {
        if (this->producer_) {
            coro_check(!this->producer_->continuation.running(),
                       "task: destroyed while its coroutine runs -- dropped from inside its own chain",
                       detail::site::current());
            coro_check(!this->producer_->continuation.joined(), "task: destroyed while a coroutine joins it",
                       detail::site::current());

            this->producer_->release();
        }
    }

    /// Whether there is a task in it: `false` for an empty one.
    inline explicit operator bool() const noexcept { return this->producer_ != nullptr; }

    /// \return `true` once the result is here: the coroutine has run to its end, by
    ///         reaching it or by leaving through an exception; the loop has taken the
    ///         operation back. Not to be asked of an empty task.
    inline bool done() const noexcept {
        coro_check(this->producer_, "task: empty -- default-constructed or moved-from",
                   detail::site::current());
        return this->producer_->ready();
    }

    /// \return the value, moved out: ask once, after done().
    /// \throw whatever left the producer.
    /// \param where the caller's line, for a strict build's report; left to its default.
    inline R result(detail::site where = detail::site::current()) requires(!std::is_void_v<R>) {
        check_ended(where);
        return this->producer_->take_result(where);
    }

    /// \throw whatever left the producer. Ask after done(), as often as needed.
    /// \param where the caller's line, for a strict build's report; left to its default.
    inline void result(detail::site where = detail::site::current()) const
        requires(std::is_void_v<R>)
    {
        check_ended(where);
        this->producer_->take_result(where);
    }

    /// How it ended, asked without throwing -- the way std::future and wxl's future are asked.
    /// Each is for a task that has ended (done()); asked earlier, or of an empty task, it is a
    /// broken rule, which a build that checks stops at, as result() is. A build that does not
    /// check still answers truthfully before the end: neither a value nor an exception yet,
    /// both false -- the flag of the end is read first, and the answer only after it.
    ///
    /// \return `true` if the body ended without an exception: with its value, for a task<T> --
    ///         whether or not result() has taken it since -- and simply without a failure, for a
    ///         task<>. Once the body has ended, exactly one of has_value() and has_exception()
    ///         holds; before, neither.
    /// \param where the caller's line, for a strict build's report; left to its default.
    inline bool has_value(detail::site where = detail::site::current()) const noexcept {
        check_answered(where);
        return this->producer_->ready() && !this->producer_->has_exception();
    }

    /// \return `true` if the body ended with an exception -- its own, a failure of the work it
    ///         stood for, or the cancellation that cut it short.
    /// \param where the caller's line, for a strict build's report; left to its default.
    inline bool has_exception(detail::site where = detail::site::current()) const noexcept {
        check_answered(where);
        return this->producer_->ready() && this->producer_->has_exception();
    }

    /// \return what left the producer, not thrown: ask once has_exception() has said so --
    ///         of a task that ended with its value it is a broken rule, as with wxl's future.
    ///         result() and co_await still throw it, as often as they are asked.
    /// \param where the caller's line, for a strict build's report; left to its default.
    inline const std::exception_ptr& get_exception_ptr(detail::site where = detail::site::current()) const noexcept {
        check_answered(where);
        coro_check(this->producer_->has_exception(), "task: get_exception_ptr() asked of a task that ended with its value",
                   where);
        return this->producer_->exception();
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
    inline friend detail::join_awaiter<R> operator co_await(Self&& kept) noexcept {
        return detail::join_awaiter<R>(kept.producer_);
    }

private:
    friend promise_type;

    inline explicit task(async_op_t<R>* producer) noexcept : detail::awaiter<R>(producer) {}

    inline void check_ended([[maybe_unused]] detail::site where) const noexcept {
        coro_check(this->producer_, "task: empty -- default-constructed or moved-from", where);
        coro_check(this->producer_->ready(), "task: result() asked before it ended", where);
    }

    inline void check_answered([[maybe_unused]] detail::site where) const noexcept {
        coro_check(this->producer_, "task: empty -- default-constructed or moved-from", where);
        coro_check(this->producer_->ready(), "task: how it ended asked before it ended", where);
    }
};

/// How many frames of coroutines returning a task exist -- made and not yet unmade, suspended
/// or running. A frame lasts as long as its body, so this is the count of bodies under way;
/// a task whose body has ended holds only its result, and is not counted. The end of a
/// process expects it at zero: every task lives in a place of an owner, which lets go
/// before then.
///
/// Only a build that checks counts, so only there does a call compile; elsewhere the
/// static_assert stops it. A reader that has to build both ways asks under `if constexpr
/// (detail::checked)`, whose discarded branch does not instantiate the call.
template <class Unused = void>
inline std::size_t live_frames() noexcept {
    static_assert(detail::checked && std::is_void_v<Unused>,
                  "live_frames(): frames are counted only in a build that checks coroutines");
    return detail::task_frames::live;
}

template <class R>
inline task<R> detail::task_promise<R>::get_return_object() {
    this->producer = new coroutine_op<R>(std::coroutine_handle<task_promise>::from_promise(*this));
    this->producer->continuation.set_running(true);
    return task<R>(this->producer);
}

}  // export namespace wxl::async
