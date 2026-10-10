module;

#include "abi.h"
#include "coroutine_checks.h"

export module wxl.async:async_op;

import :cancellation;
import :coroutine_checks;
import wxl.core;
import std;

export namespace wxl::async {

/// Says of an operation that it touches nothing but what it owns -- no buffer in a
/// coroutine frame, no object the coroutine holds -- so that a task going away early
/// may leave it to finish alone instead of waiting for it. Opening a file is the model: it
/// carries its own copy of the path, and what it brings back is the open file.
///
/// Given up, such an operation lets go of what it made at once, so that whoever gave it
/// up can ask for the same file the next moment: a result already there is destroyed by
/// the thread giving it up, and one still being made by the thread making it, as soon as
/// it is made. Which means the result of an orphanable body is destroyed on either
/// thread, and has to be something that can be.
struct orphanable_t {
    explicit orphanable_t() = default;
};

inline constexpr orphanable_t orphanable{};

namespace task_detail {

using coro_detail::site;

/// The coroutine awaiting a result, in one word. The handle's address is that of a frame,
/// which starts with pointers, so its two lowest bits are free; where the checks are
/// compiled in they say whether the waiter stands in a join_awaiter and whether the body
/// of the coroutine making the result is on the stack. Without the checks the bits are
/// never set, and the word is the handle and nothing else -- kept as a pointer, as the
/// handle keeps it, so that the compiler treats it the same.
class continuation_word
{
public:
    /// Whom final_suspend hands the thread to: the waiter, or nobody.
    inline std::coroutine_handle<> next() const noexcept {
        const std::uintptr_t handle = bits() & ~flags;
        return handle ? std::coroutine_handle<>::from_address(reinterpret_cast<void*>(handle))
                      : std::noop_coroutine();
    }

    /// The waiter, or an empty handle: for whoever resumes it from outside a coroutine.
    inline std::coroutine_handle<> waiter() const noexcept {
        return std::coroutine_handle<>::from_address(reinterpret_cast<void*>(bits() & ~flags));
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

    void* word_ = nullptr;
};

}  // namespace task_detail

/// One result in the making, which a `task` waits for: the value or the exception, the
/// coroutine waiting, and whether the result is there yet. A task reads these the same
/// way whatever makes the result, and two kinds of producer make one:
///
/// - **An operation**, carried out elsewhere: its body (`execute()`) runs on the worker
///   thread, on the system's thread pool, or here inside the call that starts it. It is
///   born on the STA thread inside that call, travels out and back as a plain pointer,
///   and dies on the STA thread; the worker in between neither creates nor destroys it --
///   which is why the memory comes from sta_memory_pool. The loop takes it out of the
///   return channel and wakes the coroutine waiting for it (`come_back()`).
/// - **A coroutine**, whose promise is this object too (`task<R>::promise_type`): it runs
///   where it is resumed, on this thread, is never sent anywhere, and its end hands the
///   thread to the coroutine waiting for it by symmetric transfer.
///
/// Waiting -- is it there, park the waiter, take the result -- is the same plain code for
/// both, over the fields here. What differs is what the owner letting go does, one call
/// through the virtual table (`release()`): an operation is deleted or given up, a
/// coroutine's frame is destroyed. A request to end early reaches only an operation -- one
/// made in the form with a token (`cancellation_detail::operation_under`); a coroutine
/// ends by the token it was given, by its own code.
///
/// The answer is what happened to the operation, since nothing orders a request against
/// work on another thread: the cancellation is the answer of one cut short or never
/// started, and one that got there first answers with what it made, or with its own
/// failure.
///
/// The task owns it, while the channels carry a borrowed pointer. So a task cannot
/// simply delete an operation that is still out: when it goes away first -- its frame
/// unwinding on an exception, its owner dropping it, or nobody ever awaiting it -- it gives
/// the operation up instead. The operation is asked to cancel, the task waits until the
/// worker has let go of it (unless it is `orphanable`), and the loop deletes it when it
/// comes back. That wait is what keeps a buffer in the frame from being written after the
/// frame is gone -- the same guarantee a synchronous read on an ordinary stack gives.
class async_op : public core::noncopyable
{
public:
    async_op() noexcept = default;

    inline explicit async_op(orphanable_t) noexcept : orphanable_(true) {}

    virtual ~async_op() = default;

    /// Created and destroyed on the STA thread, both, so the pool is where it belongs --
    /// for a coroutine, the whole frame, which the promise's allocation functions are
    /// asked for. The destructor is virtual, so `delete` through this base reaches the
    /// most derived type's deallocation and hands the pool the size it actually gave out.
    inline static void* operator new(std::size_t size) {
        return core::sta_memory_pool::alloc(size);
    }

    inline static void operator delete(void* mem, std::size_t size) noexcept {
        core::sta_memory_pool::free(mem, size);
    }

    /// Whether the result is here: an operation the loop has taken out of the return
    /// channel -- not merely one that has finished, which may still be in the channel and
    /// is not the coroutine's until the loop takes it out -- or a coroutine at its end.
    inline bool ready() const noexcept { return ready_; }

    /// The coroutine waiting for the result: parked by the co_await of a task, woken by
    /// whoever makes the result. One at most, and on the STA thread alone.
    task_detail::continuation_word continuation;

    /// The owner lets go: the STA thread's call, from the task's destructor. An operation
    /// that is back goes; one still out is given up (`abandon`). A coroutine overrides it
    /// and destroys its frame -- and since coroutine_handle::destroy() is not noexcept,
    /// neither is this: the override ends in that call, where a noexcept one would have to
    /// guard it.
    virtual void release() {
        if (ready_)
            delete this;
        else
            abandon(this);
    }

    /// The STA thread's call, while the operation is out: asks it to stop, the way giving
    /// it up does, and keeps it. It still comes back, and whoever waits for it still
    /// waits, only not as long: one not started does not start, and one running is
    /// interrupted by on_cancel() if it can be. Asking twice asks once.
    ///
    /// The flag and on_cancel() are all it does: what the operation then answers, it
    /// answers itself -- the cancellation if it was cut short or never started, its own
    /// result if it got there first.
    inline void cancel() noexcept {
        if (canceled_.load(std::memory_order_relaxed)) return;

        // Ordered against the worker's second look at the flag, which it takes after
        // handing an operation to the kernel.
        canceled_.store(true, std::memory_order_seq_cst);
        on_cancel();
    }

    /// The worker thread's call: runs the body and keeps whatever it threw, since
    /// there is nobody here to throw to -- the exception belongs to the coroutine
    /// and travels back to it in the op itself.
    ///
    /// An operation asked to stop before the worker reached it is not started, and
    /// answers the cancellation: written here, before it goes back, so that the channel
    /// carries the answer the way it carries any other. Given up, it is deleted with the
    /// answer unread. The flag is read here, once, with an ordinary load; the path that
    /// finds it set is the one that pays.
    ///
    /// \return `true` if the operation is finished and goes back to the STA
    ///         thread. A body that is not done answers `false`, and the operation
    ///         goes nowhere: it now belongs to whatever it is waiting for -- the
    ///         OS, a readiness notification, another queue -- and comes back here
    ///         when that fires. Which means this is not called once per operation:
    ///         a socket that has taken only part of a message is executed again,
    ///         and a body has to be written knowing it. Nor may it fail to come
    ///         back: a task giving it up waits for it.
    inline bool packaged_execute() noexcept {
        if (canceled_.load(std::memory_order_relaxed)) [[unlikely]] {
            answer_canceled();
            return true;
        }

        try {
            return execute();
        } catch (...) {
            error_ = std::current_exception();
            return true;
        }
    }

    /// The STA thread's call, as the loop takes the operation out of the return channel:
    /// deletes it if it was given up, resumes the coroutine waiting for it if there is
    /// one, and otherwise leaves it for the co_await still to come, which then finds it
    /// ready and does not suspend.
    ///
    /// \return whether a coroutine was resumed.
    ///
    /// \warning The op may be gone by the time this returns: a resumed coroutine goes
    ///          on from its co_await, and the task holding the op dies with it.
    inline bool come_back() {
        if (abandoned_) [[unlikely]] {
            delete this;
            return false;
        }

        ready_ = true;

        const std::coroutine_handle<> waiter = continuation.waiter();

        if (!waiter) return false;

        waiter.resume();
        return true;
    }

    /// The same without resuming anybody: for a loop that is being stopped, where a
    /// coroutine suspended here stays where it is and the op waits for its task to delete
    /// it along with the frame.
    inline void settle() noexcept {
        if (abandoned_) {
            delete this;
            return;
        }

        ready_ = true;
    }

    /// The STA thread's call for a result that is here as it stands: an operation over
    /// inside the call that started it, on this thread, which never travels -- or a
    /// coroutine at its end.
    inline void deliver_here() noexcept { ready_ = true; }

    /// The task's call, when it goes away before the operation has been delivered:
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

    /// Called on the STA thread when the operation is asked to stop while it is still
    /// out -- given up by its task, or cancelled under a token -- after the flag
    /// packaged_execute() reads has been set. Once, but for an orphanable operation
    /// cancelled by its token and then given up, which hears both, and abandoned() says which.
    ///
    /// For an operation the worker has not reached, the flag is all it takes. One already
    /// running is interrupted here, if what it waits for can be interrupted -- the way
    /// CancelIoEx completes a read the kernel is holding -- so that the task, which
    /// waits for it to come back, does not wait longer than it has to. One interrupted
    /// answers the cancellation itself, and one that finished first, its result. Must not
    /// throw: it runs in a destructor, often one called by unwinding.
    virtual void on_cancel() noexcept {}

    inline void rethrow_if_failed() const {
        if (error_) std::rethrow_exception(error_);
    }

    /// For an operation that learns of its failure without throwing -- from the code an
    /// overlapped call left behind -- and for a coroutine, whose failure the promise
    /// catches.
    inline void set_error(std::exception_ptr error) noexcept { error_ = std::move(error); }

    /// The answer becomes the cancellation: for an operation cut short or never started,
    /// on whichever thread finds that out. One exception made once and shared, since the
    /// answer is always the same and its path the only one that pays for it (async_op.cpp).
    void answer_canceled() noexcept;

    /// Keeps the exception being handled as the answer -- unless it is a call to the
    /// system that the cutting short ended, `system_exception` with ERROR_OPERATION_ABORTED,
    /// which answers the cancellation. Called from inside a handler (async_op.cpp).
    void keep_failure() noexcept;

    /// Whether it has been asked to stop: given up or cancelled. For a body that hands the
    /// operation to the kernel: read after the handing over, it closes the race with a
    /// cancellation that came before there was anything to cancel.
    inline bool canceled() const noexcept { return canceled_.load(std::memory_order_seq_cst); }

    /// Whether its task has given it up. The STA thread's alone, like everything the
    /// worker does not read.
    inline bool abandoned() const noexcept { return abandoned_; }

private:
    template <class R>
    friend class async_op_t;

    // The flags lie between the waiter's word and the exception. A constructor zeroes all
    // three, and a compiler may make the flags' few bytes with one word-wide store that
    // reaches into a neighbour; the exception, read at every co_await, is then not the
    // one it reaches into. A read spanning two stores still in flight is not forwarded
    // and waits for both: clang, the other way round, paid 3 ns a co_await for it.

    /// The result is here: taken out of the return channel, or the coroutine has ended.
    bool ready_ = false;

    /// Given up by its task: whoever takes it out of the return channel deletes it. The
    /// one flag come_back() reads before the waiter.
    bool abandoned_ = false;

    const bool orphanable_ = false;

    /// The value has been taken: read by a build that checks coroutines, to stop at a
    /// second take. In the padding after the flags, so no build pays a byte for it.
    bool taken_ = false;

    /// Written by the STA thread, read by the worker before the body: the one field
    /// of the operation both threads touch while it is out, hence atomic.
    std::atomic<bool> canceled_{false};

    std::exception_ptr error_;
};

/// What a task of R waits for: a producer with a result of type R -- the value the body
/// made, kept until the coroutine waiting for it comes back for it.
template <class R>
class async_op_t : public async_op
{
public:
    using result_type = R;

    using async_op::async_op;

    /// The STA thread's call, at the end of a co_await, or from task::result().
    /// \return the value, moved out: it goes once, and a build that checks coroutines
    ///         stops at a second take.
    /// \throw whatever the body threw, each time it is asked.
    /// \param where the place of the co_await, for a strict build's report.
    R take_result([[maybe_unused]] task_detail::site where = task_detail::site::current()) {
        rethrow_if_failed();

        if constexpr (coro_detail::checked) {
            coro_check(!taken_, "task: the value has already been taken", where);
            taken_ = true;

            // Finished, did not throw, and left nothing behind: a body that answered
            // "done" without producing what it promised.
            coro_check(value_.has_value(), "task: ended without a result", where);
        }

        return std::move(*value_);
    }

protected:
    void set_value(R&& value) { value_.emplace(std::move(value)); }

    /// Destroys what was produced, for an operation nobody is going to ask.
    void drop_value() noexcept { value_.reset(); }

private:
    // An optional rather than an R: a result type is not obliged to have a default
    // constructor, and an open file has no sensible empty state anyway.
    std::optional<R> value_;
};

template <>
class async_op_t<void> : public async_op
{
public:
    using result_type = void;

    using async_op::async_op;

    /// Nothing is moved out, so asking again is asking again: what the body threw is
    /// thrown each time.
    inline void take_result([[maybe_unused]] task_detail::site where = task_detail::site::current()) {
        rethrow_if_failed();
    }

protected:
    inline void drop_value() noexcept {}
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

/// Where an orphanable operation is, as the threads that may end it agree: the one
/// carrying it out, and the STA thread cancelling it or giving it up, meet on this
/// word.
///
/// Cancelling an operation that is running, or giving it up, also cuts short the
/// call to the system it stands in -- an opening the system is taking its time over fails
/// at once, as cancelled. That reaches a thread and not an operation, so it is only ever
/// done to a thread known to be inside this operation's body: the body does not leave
/// while it is being done.
///
/// Cancelled, the operation is still somebody's: what the body makes is its answer -- the
/// cancellation if the body was cut short, its result if it finished first. Given up, what
/// it makes is nobody's, and is destroyed on whichever thread has it.
class orphan_stage
{
public:
    /// The carrying thread's call before the body.
    /// \return `false` if the operation was cancelled or given up before it started, and is
    ///         not to.
    [[nodiscard]] bool enter() noexcept;

    /// The carrying thread's call after the body, whichever way the body ended.
    /// \return `false` if the operation was given up while it ran: what it made is
    ///         nobody's, and the caller destroys it. One cancelled keeps it.
    [[nodiscard]] bool leave() noexcept;

    /// The call of the thread giving the operation up, cancelled before or not.
    /// \return `true` if the body had finished: what it made is there, and the caller
    ///         destroys it.
    [[nodiscard]] bool give_up() noexcept;

    /// The call of the thread whose token cancelled the operation: one not started
    /// does not start, one running has the call it stands in cut short, and one finished
    /// keeps what it made.
    void cancel() noexcept;

    /// The body's question between two calls to the system, when it makes more than
    /// one: cutting short reaches the call under way and no other, so a body cancelled or
    /// given up between two of its calls would go on to the next unless it asks. A body
    /// that stops on it has not finished, and answers operation_canceled_exception.
    inline bool cut_short() const noexcept {
        return stage_.load(std::memory_order_relaxed) >= stage::cutting_short;
    }

private:
    enum class stage : std::uint8_t {
        waiting,
        running,
        finished,
        /// Cancelled or given up while running, and the call the body stands in is being cut
        /// short this moment: the body waits for that to be over before it leaves.
        cutting_short,
        /// Cancelled while running, its call cut short: what the body makes is its answer.
        canceled,
        /// Given up, or cancelled before it started: the body does not run, or what it makes
        /// is nobody's.
        given_up,
    };

    /// Cuts short the call the body's thread stands in, if it stands in one.
    void cut_the_call_short() noexcept;

    std::atomic<stage> stage_{stage::waiting};

    /// The thread the body runs on, written before the word says `running`.
    std::uint32_t thread_ = 0;
};

/// A body that takes the stage, to ask it between its calls.
template <class Fn>
concept asks_stage = std::invocable<Fn&, const orphan_stage&>;

template <class Fn>
struct orphan_result
{
    using type = std::invoke_result_t<Fn&>;
};

template <asks_stage Fn>
struct orphan_result<Fn>
{
    using type = std::invoke_result_t<Fn&, const orphan_stage&>;
};

/// What an orphanable body answers, whether or not it takes the stage.
template <class Fn>
using orphan_result_t = orphan_result<Fn>::type;

/// An orphanable operation whose body is a lambda.
template <class Fn, class R = orphan_result_t<Fn>>
class orphan_op_f : public async_op_t<R>
{
public:
    template <class Fn2>
    explicit orphan_op_f(Fn2&& fn) : async_op_t<R>(orphanable), fn_(std::forward<Fn2>(fn)) {}

protected:
    bool execute() override {
        // Cancelled or given up before it started: it never runs, and answers the
        // cancellation.
        if (!stage_.enter()) [[unlikely]] {
            this->answer_canceled();
            return true;
        }

        // Caught here rather than by the caller: the stage has to be settled on the way
        // out of a body that threw as well.
        try {
            if constexpr (std::is_void_v<R>)
                run();
            else
                this->set_value(run());
        } catch (...) {
            this->keep_failure();
        }

        if (!stage_.leave()) [[unlikely]]
            this->drop_value();

        return true;
    }

    /// Cancelled, the body is cut short and keeps its answer; given up -- cancelled before
    /// or not -- it lets go of what it made.
    void on_cancel() noexcept override {
        if (!this->abandoned()) {
            stage_.cancel();
            return;
        }

        if (stage_.give_up()) this->drop_value();
    }

private:
    inline R run() {
        if constexpr (asks_stage<Fn>)
            return fn_(std::as_const(stage_));
        else
            return fn_();
    }

    Fn fn_;
    orphan_stage stage_;
};

namespace cancellation_detail {

/// An operation of this module in its form with a token: `Op` as it is, standing under the
/// token from the moment it is made until it goes. Cancelled while it is out, it is asked to
/// stop (`async_op::cancel()`), and answers what then happens to it; back already, it keeps
/// its answer, and given up, it hears nothing: nobody waits for it. Its place in the token's
/// event is built into it, so standing allocates nothing, and the token is held, so the
/// event outlives the stand.
/// Made only under a token that has a source and is not cancelled yet: under one cancelled
/// already the forms make nothing at all.
template <class Op>
class operation_under final : public Op
{
public:
    template <class... Args>
    inline explicit operation_under(cancellation_token stop, Args&&... args)
        : Op(std::forward<Args>(args)...), stop_(std::move(stop)), node_(*this) {
        state_of(stop_)->event().add(core::as_not_null<cancellation_event::func_t>(&node_));
    }

    /// Out of the event before the token goes. Once the token has been cancelled the event
    /// the node stood in has been fired and is gone, and remove() finds nothing.
    inline ~operation_under() override {
        state_of(stop_)->event().remove(
            core::cookie_t{static_cast<cancellation_event::func_t*>(&node_)});
    }

private:
    /// The operation's place in the token's event.
    class node final : public cancellation_event::func_t
    {
    public:
        inline explicit node(operation_under& op) noexcept : op_(op) {}

        /// The token was cancelled: an operation still out and still its task's is asked to
        /// stop; one back already or given up is left as it is. Resumes nobody.
        inline void operator()() noexcept override {
            if (!op_.ready() && !op_.abandoned()) op_.cancel();
        }

        /// The operation owns its node.
        inline void release() noexcept override {}

    private:
        operation_under& op_;
    };

    cancellation_token stop_;
    node node_;
};

}  // namespace cancellation_detail

}  // export namespace wxl::async
