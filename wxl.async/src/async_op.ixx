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

/// What owns an operation, defined with the coroutines (wxl.async:task): declared here to be
/// the one reader of an answer that is not thrown (async_op::exception()).
template <class R>
class task;

namespace detail {

/// The coroutine awaiting a result, in one word. The handle's address is that of a frame,
/// which starts with pointers, so its two lowest bits are free; where the checks are
/// compiled in they say whether the waiter stands in a join_awaiter and whether the body
/// of the coroutine making the result is on the stack. Without the checks the bits are
/// never set, and the word is the handle and nothing else -- kept as a pointer, as the
/// handle keeps it, so that the compiler treats it the same.
class continuation_word
{
public:
    /// The waiter, or an empty handle.
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
        if constexpr (detail::checked) set(running ? bits() | running_bit : bits() & ~running_bit);
    }

private:
    static constexpr std::uintptr_t joined_bit = detail::checked ? 1 : 0;
    static constexpr std::uintptr_t running_bit = detail::checked ? 2 : 0;
    static constexpr std::uintptr_t flags = joined_bit | running_bit;

    inline std::uintptr_t bits() const noexcept { return reinterpret_cast<std::uintptr_t>(word_); }
    inline void set(std::uintptr_t bits) noexcept { word_ = reinterpret_cast<void*>(bits); }

    void* word_ = nullptr;
};

}  // namespace detail

/// One result in the making, which a `task` waits for: the value or the exception, the
/// coroutine waiting, and whether the result is there yet. A task reads these the same
/// way whatever makes the result, and two kinds of producer make one:
///
/// - **An operation**, carried out elsewhere: its body (`execute()`) runs on the worker
///   thread, on the system's thread pool, or here inside the call that starts it. It is
///   born on the STA thread inside that call, travels out and back as a plain pointer,
///   and dies on the STA thread; the worker in between neither creates nor destroys it --
///   which is why the memory comes from sta_memory_pool. The loop takes it out of the
///   return channel and wakes the coroutine waiting for it (`complete()`). One the platform
///   carries out -- wxl.ui's, for a WinRT asynchronous operation -- has no body to run: the
///   call its thread's dispatcher queue makes answers it and calls `complete()`.
/// - **A coroutine** returning a task (`detail::coroutine_op`), whose promise writes the
///   result here: it runs where it is resumed, on this thread, is never sent anywhere, and
///   its end takes its frame down and hands the thread to the coroutine waiting for it by
///   symmetric transfer. This object outlives the frame, for the task to read.
///
/// Waiting -- is it there, park the waiter, take the result -- is the same plain code for
/// both, over the fields here. What differs is what the owner letting go does, one call
/// through the virtual table (`release()`): an operation is deleted or given up, a
/// coroutine still in flight is stopped. A request to end early reaches only an operation -- one
/// made in the form with a token (`detail::cancellable_operation`); a coroutine
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
/// worker has let go of it (unless it is orphanable, `orphanable_op_f`), and the loop
/// deletes it when it comes back. That wait is what keeps a buffer in the frame from being
/// written after the frame is gone -- the same guarantee a synchronous read on an ordinary
/// stack gives.
class async_op : public core::noncopyable
{
public:
    async_op() noexcept = default;

    /// An error kept is released here; a value kept is the derived producer's to destroy
    /// (async_op_t). Neither is there for a coroutine stopped in flight or an operation never
    /// run.
    virtual ~async_op() {
        if (error_kept()) std::destroy_at(&kept_error());
    }

    /// Created and destroyed on the STA thread, both, so the pool is where it belongs. The
    /// destructor is virtual, so `delete` through this base reaches the
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
    detail::continuation_word continuation;

    /// The owner lets go: the STA thread's call, from the task's destructor. An operation
    /// that is back goes; one still out is given up (`abandon`). An orphanable operation
    /// overrides it, and is given up without the wait. A coroutine overrides it, and
    /// destroys the frame of a body still in flight -- and since
    /// coroutine_handle::destroy() is not noexcept, neither is this: a noexcept override
    /// would have to guard that call.
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
            set_current_exception();
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
    inline bool complete() {
        if (abandoned_) [[unlikely]] return complete_without_task();

        ready_ = true;

        const std::coroutine_handle<> waiter = continuation.waiter();

        if (!waiter) return false;

        waiter.resume();
        return true;
    }

    /// The STA thread's call for a result that is here as it stands: an operation over
    /// inside the call that started it, on this thread, which never travels -- or a
    /// coroutine at its end.
    inline void complete_in_place() noexcept { ready_ = true; }

    /// The task's call, when it goes away before the operation has been delivered:
    /// marks it given up, asks it to cancel, and waits, without resuming anybody, until
    /// the worker has let go of it. The loop completes it when it comes back, which deletes
    /// it with what it made, or this does, if it is next in line already. An orphanable
    /// operation (`orphanable_op_f`) overrides release() and is not waited for.
    ///
    /// Defined with the loop, whose return channel it waits on (sta_loop.cpp).
    static void abandon(async_op* op) noexcept;

    inline bool has_exception() const noexcept { return error_kept(); }

protected:
    /// What the work threw, or its cancellation; empty for work that answered with its
    /// result. Its task's alone, and only once the result is here: before that the worker
    /// may be writing it, and whoever holds a borrowed pointer to the operation would read
    /// it in a race.
    inline const std::exception_ptr& exception() const noexcept {
        return *std::launder(reinterpret_cast<const std::exception_ptr*>(error_));
    }

    /// The work itself, on the worker thread. \see packaged_execute().
    virtual bool execute() = 0;

    /// What complete() does with an operation no task holds: one given up is completed and
    /// deleted, its answer with it. One whose answer belongs to another producer of this
    /// thread hands it over first and resumes that producer's waiter. Off the path of every
    /// operation a task holds, which is why it may cost a call through the virtual table.
    /// \return whether a coroutine was resumed.
    virtual bool complete_without_task() {
        ready_ = true;
        delete this;
        return false;
    }

    /// Gives the operation up without waiting for it, for one that may finish alone: marked
    /// given up and asked to stop, and left for the loop to delete when it comes back. Asked
    /// even when its token has asked already: cancelled, it kept what it made, and given up,
    /// it lets go of that too.
    inline void abandon_without_waiting() noexcept {
        abandoned_ = true;
        canceled_.store(true, std::memory_order_seq_cst);
        on_cancel();
    }

    /// For an operation no task is ever to hold, said before it is sent: the loop taking it
    /// back hands it to complete_without_task(), as it does one given up.
    inline void start_without_task() noexcept { abandoned_ = true; }

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
        if (error_kept()) std::rethrow_exception(exception());
    }

    /// For an operation that learns of its failure without throwing -- from the code an
    /// overlapped call left behind.
    inline void set_error(std::exception_ptr error) noexcept {
        coro_check(error != nullptr, "async_op: answered with an empty exception_ptr",
                   detail::site::current());
        ::new (error_slot()) std::exception_ptr(std::move(error));
    }

    /// Keeps the exception being handled as the answer, made in place: the one call that
    /// takes it, with no copy after. Called from inside a handler -- by the worker for what a
    /// body threw, and by a coroutine's promise for what left the body.
    inline void set_current_exception() noexcept {
        ::new (error_slot()) std::exception_ptr(std::current_exception());
    }

    /// The answer becomes the cancellation: for an operation cut short or never started,
    /// on whichever thread finds that out. One exception made once and shared, since the
    /// answer is always the same and its path the only one that pays for it (async_op.cpp).
    void answer_canceled() noexcept;

    /// Keeps the exception being handled as the answer -- unless it is a call to the
    /// system that the cutting short ended, `last_error` with ERROR_OPERATION_ABORTED,
    /// which answers the cancellation. Called from inside a handler (async_op.cpp).
    void answer_current_exception() noexcept;

    /// Whether a value is held: for a build that checks coroutines, which alone keeps it.
    inline bool value_made() const noexcept { return made_; }

    /// Whether it has been asked to stop: given up or cancelled. For a body that hands the
    /// operation to the kernel: read after the handing over, it closes the race with a
    /// cancellation that came before there was anything to cancel.
    inline bool stop_requested() const noexcept {
        return canceled_.load(std::memory_order_seq_cst);
    }

    /// Whether its task has given it up, or it was started without a task
    /// (start_without_task()). The STA thread's alone, like everything the worker does not
    /// read.
    inline bool abandoned() const noexcept { return abandoned_; }

private:
    template <class R>
    friend class async_op_t;

    template <class R>
    friend class task;

    // The flags lie between the waiter's word and the exception. A constructor zeroes all
    // three -- the exception's storage is words, value-initialized, and zero is what an empty
    // exception_ptr is, so an empty one costs no call to make, to test or to destroy. A
    // compiler may make the flags' few bytes with one word-wide store that reaches into a
    // neighbour; the exception's first word, read at every co_await, is then not the one it
    // reaches into. A read spanning two stores still in flight is not forwarded and waits
    // for both: clang, the other way round, paid 3 ns a co_await for it.

    /// The result is here: taken out of the return channel, or the coroutine has ended.
    bool ready_ = false;

    /// Given up by its task, or held by none: whoever takes it out of the return channel
    /// hands it to complete_without_task(), which deletes it or passes its answer on. The one
    /// flag complete() reads before the waiter.
    bool abandoned_ = false;

    /// The value has been taken: read by a build that checks coroutines, to stop at a
    /// second take. In the padding after the flags, so no build pays a byte for it.
    bool taken_ = false;

    /// A value is held (async_op_t): kept by a build that checks coroutines alone, which
    /// stops at a producer that ended without its value, at one whose value and readiness
    /// disagree as it goes, and at a coroutine that failed after its co_return. In the
    /// padding after the flags, as taken_ is. Written where the value is, by the thread
    /// that makes or drops it.
    bool made_ = false;

    /// Written by the STA thread, read by the worker before the body: the one field
    /// of the operation both threads touch while it is out, hence atomic.
    std::atomic<bool> canceled_{false};

    /// Whether an error is kept, read off its storage: an empty std::exception_ptr is all
    /// zero bytes and a kept one has a non-zero first word -- on MSVC, where it is two
    /// pointers, and on libstdc++ and libc++, where it is one, as a probe of each showed.
    /// One load and a test, where the object's own `operator bool` is a call out of line
    /// on MSVC.
    inline bool error_kept() const noexcept {
        std::uintptr_t first;
        std::memcpy(&first, error_, sizeof first);
        return first != 0;
    }

    inline std::exception_ptr& kept_error() noexcept {
        return *std::launder(reinterpret_cast<std::exception_ptr*>(error_));
    }

    /// Where an error is made: the storage, empty until then, since an operation answers
    /// once. A build that checks stops at a second answer, which would leak the first.
    inline void* error_slot() noexcept {
        coro_check(!error_kept(), "async_op: answered twice", detail::site::current());
        return error_;
    }

    /// Takes over the error `made` keeps, if it keeps one, by moving its bytes and zeroing
    /// the source: an exception_ptr is a counted reference nothing points back at, so the
    /// same bytes elsewhere are the same reference, owned once before and once after, and no
    /// count is touched. Zero bytes moved are an empty one moved.
    inline void take_error_from(async_op& made) noexcept {
        std::memcpy(error_slot(), made.error_, sizeof error_);
        std::memset(made.error_, 0, sizeof made.error_);
    }

    static_assert(sizeof(std::exception_ptr) % sizeof(void*) == 0 &&
                  alignof(std::exception_ptr) <= alignof(std::uintptr_t));

    /// What the work threw, or its cancellation: constructed here when it comes, and zero
    /// words -- an empty exception_ptr -- until then. Words rather than aligned bytes: the
    /// same size and alignment, with no alignment specifier to pad the class around.
    std::uintptr_t error_[sizeof(std::exception_ptr) / sizeof(std::uintptr_t)]{};
};

// The value's storage is aligned for R, and for an R aligned wider than the producer that
// costs padding. C4324 reports it at level 4, and for a template it fires where the class is
// laid out -- in a consumer built with /W4 -- so it is silenced here, at the declaration.
#pragma warning(push)
#pragma warning(disable : 4324)  // structure was padded due to alignment specifier
/// What a task of R waits for: a producer with a result of type R -- the value the body
/// made, kept until the coroutine waiting for it comes back for it.
///
/// The value lives in raw storage, and nothing says whether it is there but the answer
/// itself: at completion there is either an error or a value. (A build that checks keeps a
/// byte that says so too, and stops where the two disagree.) So the destructor destroys
/// the error if one is kept, the value if the producer is ready() without one, and nothing
/// otherwise -- a coroutine stopped in flight, an operation never run, or an orphanable
/// one given up, whose value went with the race that gave it up (`drop_value()`) or was
/// handed on (`take_answer_from()`).
template <class R>
class async_op_t : public async_op
{
public:
    using result_type = R;

    using async_op::async_op;

    ~async_op_t() override {
        coro_check(made_ == (!error_kept() && ready_),
                   "async_op_t: a producer ready without an error holds its value, and no other does",
                   detail::site::current());

        if constexpr (!std::is_trivially_destructible_v<R>) {
            if (!error_kept() && ready_) std::destroy_at(&value());
        }
    }

    /// The STA thread's call, at the end of a co_await, or from task::result().
    /// \return the value, moved out: it goes once, and a build that checks coroutines
    ///         stops at a second take.
    /// \throw whatever the body threw, each time it is asked.
    /// \param where the place of the co_await, for a strict build's report.
    R take_result([[maybe_unused]] detail::site where = detail::site::current()) {
        rethrow_if_failed();

        if constexpr (detail::checked) {
            coro_check(!taken_, "task: the value has already been taken", where);
            taken_ = true;

            // Finished, did not throw, and left nothing behind: a body that answered
            // "done" without producing what it promised.
            coro_check(made_, "task: ended without a result", where);
        }

        return std::move(value());
    }

protected:
    /// The answer of work that did not fail, made in place. Once: a body that has answered
    /// does not answer again.
    void set_value(R&& value) {
        coro_check(!made_, "async_op: answered twice", detail::site::current());
        ::new (static_cast<void*>(value_)) R(std::move(value));
        if constexpr (detail::checked) made_ = true;
    }

    /// Destroys what was produced, for an orphanable operation given up, which nobody is going
    /// to ask: on the thread that won the race (orphan_state), once the body has answered --
    /// with its value, or with an error, which the destructor releases.
    void drop_value() noexcept {
        if (!error_kept()) {
            std::destroy_at(&value());
            if constexpr (detail::checked) made_ = false;
        }
    }

    /// Takes over the answer another producer made -- its value or its failure -- for a
    /// producer whose work that one carried out. The STA thread's, once `made` is back, and
    /// inside the loop's call that takes it back, so it does not throw. `made` is an orphan
    /// no task holds, which its destructor finds not ready: what it answered leaves it here,
    /// the error by its bytes and the value by a move and the destruction of its own.
    void take_answer_from(async_op_t& made) noexcept {
        static_assert(std::is_nothrow_move_constructible_v<R>,
                      "async_op_t::take_answer_from: the result must move without throwing");

        if (made.error_kept()) {
            take_error_from(made);
            return;
        }

        ::new (static_cast<void*>(value_)) R(std::move(made.value()));
        std::destroy_at(&made.value());

        if constexpr (detail::checked) {
            made_ = true;
            made.made_ = false;
        }
    }

private:
    inline R& value() noexcept { return *std::launder(reinterpret_cast<R*>(value_)); }

    // Storage rather than an R: a result type is not obliged to have a default constructor,
    // and an open file has no sensible empty state anyway. Whether a value is in it is said
    // by the answer (see the class).
    alignas(R) std::byte value_[sizeof(R)];
};
#pragma warning(pop)

template <>
class async_op_t<void> : public async_op
{
public:
    using result_type = void;

    using async_op::async_op;

    /// Nothing is moved out, so asking again is asking again: what the body threw is
    /// thrown each time.
    inline void take_result([[maybe_unused]] detail::site where = detail::site::current()) {
        rethrow_if_failed();
    }

protected:
    inline void drop_value() noexcept {}

    inline void take_answer_from(async_op_t& made) noexcept { take_error_from(made); }
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

/// Where an orphanable operation is, as the thread carrying it out and the STA thread
/// cancelling it or giving it up agree: they meet on this word.
///
/// Cancelling an operation that is running, or giving it up, also cancels the synchronous
/// call to the system it stands in (CancelSynchronousIo): a call the system can cancel -- an
/// opening it is taking its time over, a write to a pipe -- fails with
/// ERROR_OPERATION_ABORTED, and one it cannot runs to its end. That reaches a thread and not
/// an operation, so it is only ever done to a thread known to be inside this operation's
/// body: the body does not leave while it is being done. Work that is not a call to the
/// system is not interrupted; a body of several steps asks stop_requested() between them.
///
/// Cancelled, the operation is still somebody's, and is waited for: what the body makes is
/// its answer. Given up, what it makes is nobody's, and is destroyed on whichever thread has
/// it.
class orphan_state
{
public:
    /// The carrying thread's call before the body.
    /// \return `false` if the operation was cancelled or given up before it started, and is
    ///         not to.
    [[nodiscard]] bool try_begin() noexcept;

    /// The carrying thread's call after the body, whichever way the body ended.
    /// \return `false` if the operation was given up while it ran: what it made is
    ///         nobody's, and the caller destroys it. One cancelled keeps it.
    [[nodiscard]] bool end() noexcept;

    /// The call of the thread giving the operation up, cancelled before or not.
    /// \return `true` if the body had finished: what it made is there, and the caller
    ///         destroys it.
    [[nodiscard]] bool abandon() noexcept;

    /// The call of the thread whose token cancelled an operation it waits for: one not
    /// started does not start, one running has the call it stands in cancelled, and one
    /// finished keeps what it made.
    void cancel() noexcept;

    /// The body's question between two of its steps: cancelling reaches the call to the
    /// system under way and nothing else, so a body cancelled or given up between two of its
    /// calls would go on to the next unless it asks. A body that stops on it throws
    /// operation_canceled_exception.
    inline bool stop_requested() const noexcept {
        return stage_.load(std::memory_order_relaxed) >= stage::cancelling_io;
    }

private:
    enum class stage : std::uint8_t {
        waiting,
        running,
        finished,
        /// Cancelled or given up while running, and the call the body stands in is being
        /// cancelled this moment: the body waits for that to be over before it leaves.
        cancelling_io,
        /// Cancelled while running, its call cancelled: what the body makes is its answer.
        canceled,
        /// Given up, or cancelled before it started: the body does not run, or what it makes
        /// is nobody's.
        abandoned,
    };

    /// Cancels the synchronous call the body's thread stands in, if it stands in one.
    void cancel_synchronous_io() noexcept;

    std::atomic<stage> stage_{stage::waiting};

    /// The thread the body runs on, written before the word says `running`.
    std::uint32_t thread_ = 0;
};

/// A body that takes its state, to ask it between its calls.
template <class Fn>
concept takes_orphan_state = std::invocable<Fn&, const orphan_state&>;

template <class Fn>
struct orphanable_result
{
    using type = std::invoke_result_t<Fn&>;
};

template <takes_orphan_state Fn>
struct orphanable_result<Fn>
{
    using type = std::invoke_result_t<Fn&, const orphan_state&>;
};

/// What an orphanable body answers, whether or not it takes its state.
template <class Fn>
using orphanable_result_t = orphanable_result<Fn>::type;

/// An orphanable operation whose body is a lambda (`orphanable_t`): given up, it is not
/// waited for. Its task letting go marks it given up and asks it to stop, and the loop
/// deletes it when it comes back; what it made is destroyed by whichever thread wins the
/// race over it (orphan_state) -- so it comes back with no value to destroy, and its
/// complete_without_task() deletes it without completing it. An error it answered is
/// released by the destructor all the same.
template <class Fn, class R = orphanable_result_t<Fn>>
class orphanable_op_f : public async_op_t<R>
{
public:
    template <class Fn2>
    explicit orphanable_op_f(Fn2&& fn) : fn_(std::forward<Fn2>(fn)) {}

    /// The task lets go: one back goes, and one still out is given up without the wait.
    void release() override {
        if (this->ready())
            delete this;
        else
            this->abandon_without_waiting();
    }

protected:
    /// The loop takes it back given up: its value is gone already, and it is deleted as it
    /// stands, not ready.
    bool complete_without_task() override {
        delete this;
        return false;
    }

    bool execute() override {
        // Cancelled or given up before it started: it never runs, and answers the
        // cancellation.
        if (!state_.try_begin()) [[unlikely]] {
            this->answer_canceled();
            return true;
        }

        // Caught here rather than by the caller: the state has to be settled on the way
        // out of a body that threw as well.
        try {
            if constexpr (std::is_void_v<R>)
                run();
            else
                this->set_value(run());
        } catch (...) {
            this->answer_current_exception();
        }

        if (!state_.end()) [[unlikely]]
            this->drop_value();

        return true;
    }

    /// Cancelled while it is waited for, the body has its call cancelled and keeps its
    /// answer; given up -- cancelled before or not -- it lets go of what it made.
    void on_cancel() noexcept override {
        if (!this->abandoned()) {
            state_.cancel();
            return;
        }

        if (state_.abandon()) this->drop_value();
    }

private:
    inline R run() {
        if constexpr (takes_orphan_state<Fn>)
            return fn_(std::as_const(state_));
        else
            return fn_();
    }

    Fn fn_;
    orphan_state state_;
};

namespace detail {

/// An operation of this module in its form with a token: `Op` as it is, standing under the
/// token from the moment it is made until it goes. Cancelled while it is out, it is asked to
/// stop (`async_op::cancel()`), and answers what then happens to it; back already, it keeps
/// its answer, and given up, it hears nothing: nobody waits for it. Its place in the token's
/// event is built into it, so standing allocates nothing, and the token is held, so the
/// event outlives the stand.
/// Made only under a token that has a source and is not cancelled yet: under one cancelled
/// already the forms make nothing at all.
template <class Op>
class cancellable_operation final : public Op
{
public:
    template <class... Args>
    inline explicit cancellable_operation(cancellation_token stop, Args&&... args)
        : Op(std::forward<Args>(args)...), stop_(std::move(stop)), node_(*this) {
        coro_check(!cancellation_state_of(stop_)->canceled(),
                   "cancellable_operation: made under a token cancelled already", detail::site::current());

        cancellation_state_of(stop_)->event().add(
            core::as_not_null<cancellation_event::func_t>(&node_));
    }

    /// Out of the event before the token goes. Once the token has been cancelled the event
    /// the node stood in has been fired and is gone, and remove() finds nothing.
    inline ~cancellable_operation() override {
        cancellation_state_of(stop_)->event().remove(
            core::cookie_t{static_cast<cancellation_event::func_t*>(&node_)});
    }

private:
    /// The operation's place in the token's event.
    class node final : public cancellation_event::func_t
    {
    public:
        inline explicit node(cancellable_operation& op) noexcept : op_(op) {}

        /// The token was cancelled: an operation still out and still its task's is asked to
        /// stop; one back already or given up is left as it is. Resumes nobody.
        inline void operator()() noexcept override {
            if (!op_.ready() && !op_.abandoned()) op_.cancel();
        }

        /// The operation owns its node.
        inline void release() noexcept override {}

    private:
        cancellable_operation& op_;
    };

    cancellation_token stop_;
    node node_;
};

}  // namespace detail

}  // export namespace wxl::async
