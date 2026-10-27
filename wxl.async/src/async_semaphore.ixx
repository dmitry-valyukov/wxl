module;

#include "coroutine_checks.h"

export module wxl.async:async_semaphore;

import :cancellation;
import :coroutine_checks;
import :sta_loop;
import wxl.core;
import std;

export namespace wxl::async {

class async_semaphore;

/// The mutex over a semaphore of one permit, in its own partition (wxl.async:async_mutex): its
/// destructor reads the count to stop at a mutex destroyed while locked.
class async_mutex;

namespace detail {

/// One co_await of async_semaphore::acquire(), built into the awaiting frame: the node of the
/// semaphore's line, the coroutine to resume, and the turn the loop resumes it by. Nothing is
/// allocated for a wait: the semaphore links the node in and out, and the loop holds it by its
/// turn.
///
/// A wait that cannot take a permit at once goes to the tail of the line and suspends. It leaves
/// the line in one of three ways, each in constant time from wherever it stands there:
///
/// - **handed a permit** by release(): from that moment the permit is the wait's, and the loop
///   is given its turn -- the waiter runs on the loop's next look, in its own stack, never inside
///   the release(), which is often a destructor;
/// - **cancelled** by its token while it stands in line: out of the line, the others keep their
///   order, and the loop is given its turn, which resumes it with the cancellation;
/// - **abandoned**: its frame is destroyed while it stands there, and the node takes itself out.
///
/// A permit handed over whose turn has not come yet is still the semaphore's to account for: a
/// cancellation or a destroyed frame then passes it on, to the next in line or back to the
/// count, as a release() of it would. A node has at most one turn in line at any time. A frame
/// destroyed while its turn is in line -- after a hand-over or a cancellation, before the loop's
/// next look -- takes the turn back out (sta_loop::dequeue_loop_turn), which walks the turns in
/// line: linear in their number, and accepted because that is the rare path.
///
/// Neither copied nor moved: the line and the loop hold it by address.
class semaphore_wait : public core::intrusive_list_node<semaphore_wait>, private loop_turn
{
public:
    /// A wait on `semaphore`, not begun: in no line, owed no turn.
    inline explicit semaphore_wait(async_semaphore& semaphore) noexcept
        : loop_turn{&semaphore_wait::take_turn},
          semaphore_(semaphore),
          waiter_(),
          stage_(stage::idle) {}

    /// The frame goes. A wait that is over has nothing left to do here; one still in line takes
    /// itself out, and one owed a turn takes its turn out of line -- and, handed a permit,
    /// passes it on. After this nothing in the loop or in the semaphore points into the frame.
    inline ~semaphore_wait() {
        if (linked() || stage_ != stage::idle) [[unlikely]]
            abandon();
    }

    /// Takes a free permit and does not suspend -- a free permit means nobody stands in line.
    /// Cancelled already (cancel() before the wait began), it does not suspend either, and takes
    /// nothing: the wait under the token answers the cancellation.
    inline bool await_ready() noexcept;

    /// No permit free: to the tail of the line.
    inline void await_suspend(std::coroutine_handle<> waiter) noexcept;

    /// The permit is the caller's now: taken at once, or handed over before the turn that
    /// resumed it. A cancelled wait is not asked (detail::cancelable_wait).
    inline void await_resume() const noexcept {}

    /// The token fired (detail::cancelable_awaiter), on the coroutines' thread; resumes nobody.
    /// Before the wait began: it will not suspend. In line: out of it, and the loop is given the
    /// turn that resumes it. Handed a permit whose turn has not come: the permit is passed on,
    /// and the turn already in line resumes it with the cancellation.
    inline void cancel() noexcept;

private:
    // Qualified: an unqualified friend class here would declare a new one in detail.
    friend class wxl::async::async_semaphore;

    /// Where the wait stands beyond its place in the line, which linked() says.
    enum class stage : std::uint8_t {
        /// No turn in line: not begun, standing in the line, or over.
        idle,
        /// Handed a permit by release(); its turn is in line.
        handed,
        /// Cancelled while it stood in line or held a permit handed over; its turn is in line.
        canceled,
        /// Cancelled before it began: it does not suspend, and has no turn.
        canceled_before_suspend,
    };

    /// release()'s call, with the node taken out of the line: the permit is the wait's from
    /// now, and the loop is given its turn.
    inline void hand_over() noexcept {
        coro_check(stage_ == stage::idle, "async_semaphore: a wait handed a second permit",
                   detail::site::current());

        stage_ = stage::handed;
        sta_loop::queue_loop_turn(*this);
    }

    /// The frame went before the wait was over.
    inline void abandon() noexcept;

    /// The loop's call for the turn: the permit handed over becomes the caller's, and the
    /// waiter is resumed -- to go on, or, cancelled, to throw. The node may be gone by the time
    /// this returns: the waiter goes on from its co_await, and may end its frame.
    inline static bool take_turn(loop_turn& turn);

    async_semaphore& semaphore_;

    /// The coroutine suspended in the wait; empty until it suspends.
    std::coroutine_handle<> waiter_;

    stage stage_;
};

}  // namespace detail

/// A counting semaphore for the coroutines of one thread, waited for without blocking it: what
/// .NET calls SemaphoreSlim, with WaitAsync() and Release(). `co_await semaphore.acquire(token)`
/// takes a permit, suspending the coroutine until one is free; release() gives permits back.
///
/// core::semaphore and core::mutex block the thread that waits, and on the one thread the
/// coroutines share that is a deadlock: the coroutine holding the permit would never run to
/// release it. This one suspends the coroutine instead, and the thread goes on with the rest.
///
/// **First come, first served.** A coroutine that finds no permit free stands in a line, and a
/// permit given back goes to the first in it -- handed over, not left in the count for whoever
/// asks next: while anybody stands in line, try_acquire() answers false and a later acquire()
/// stands at the tail. The waiter handed a permit is resumed on the loop's next turn, never
/// inside the release(): a release is often a destructor's (async_mutex::guard), and the code
/// after it -- the holder's own -- runs to its end first.
///
/// **A wait ends by its token.** It may last as long as another coroutine pleases, so acquire()
/// takes a token, and the call site says what ends the wait. Cancelled, a coroutine standing in
/// line leaves it -- the others keep their order, the count is untouched -- and its co_await ends
/// with operation_canceled_exception on the loop's next turn. One handed a permit whose turn has
/// not come passes the permit on before it throws; under a token cancelled already nothing is
/// taken and the co_await throws at once. A frame destroyed while it waits -- its task reset by
/// its owner -- takes itself out of the line the same way. What the holder of a permit does with
/// its own token is none of the semaphore's business.
///
/// Nothing is allocated: the line is a doubly linked list of nodes built into the awaiting
/// frames, and the loop holds a waiter by a turn built into the same node.
///
/// One thread, the coroutines': the fields are plain, and a build that checks coroutines stops
/// at a call from any other thread. It stops also at what has no sound continuation -- a count
/// released over its maximum, a semaphore destroyed while somebody waits on it -- instead of
/// tidying it away. Not recursive, and without a timeout: what ends a wait early is its token.
class async_semaphore : public core::noncopyable
{
public:
    /// What `co_await semaphore.acquire(token)` waits in: made in place in the awaiting frame,
    /// and neither copied nor moved, since the semaphore's line and the loop hold it by address.
    /// The wait is registered with the token while it is suspended
    /// (detail::cancelable_wait), by a registration built into it.
    class [[nodiscard("an acquire takes nothing until it is co_awaited")]] acquire_awaiter
        : public detail::cancelable_wait<acquire_awaiter>
    {
        friend class detail::cancelable_wait<acquire_awaiter>;
        friend class async_semaphore;

    public:
        acquire_awaiter(const acquire_awaiter&) = delete;
        acquire_awaiter& operator=(const acquire_awaiter&) = delete;

        /// A frame destroyed while the wait is registered takes it out of the token's event
        /// before the token goes; the node then takes itself out of the line.
        inline ~acquire_awaiter() { this->unregister_from_token(); }

    private:
        inline acquire_awaiter(async_semaphore& semaphore, cancellation_token token) noexcept
            : token_(std::move(token)), wait_(semaphore) {}

        inline detail::semaphore_wait& awaiter() noexcept { return wait_; }

        inline detail::cancellation_state* state() const noexcept {
            return detail::cancellation_state_of(token_);
        }

        cancellation_token token_;
        detail::semaphore_wait wait_;
    };

    /// \param count the permits free now.
    /// \param max the ceiling release() may never take the permits not held over; unbounded by
    ///        default. `0 <= count <= max`.
    /// \param where the caller's line, for a strict build's report; left to its default.
    inline explicit async_semaphore(std::ptrdiff_t count,
                                    std::ptrdiff_t max = std::numeric_limits<std::ptrdiff_t>::max(),
                                    [[maybe_unused]] detail::site where = detail::site::current()) noexcept
        : count_(count), handed_(0), max_(max), waiters_() {
        coro_check(core::sta_memory_pool::is_safe(),
                   "async_semaphore: made on a thread other than its coroutines'", where);
        coro_check(0 <= count && count <= max, "async_semaphore: a count outside [0, max]", where);
    }

    /// Nobody may wait on it any more, nor be owed a permit handed over: such a waiter would be
    /// resumed into a semaphore that is gone, and a build that checks stops here rather than
    /// let it. Where a destructor is called nothing gives the line, so the check names its own.
    inline ~async_semaphore() {
        coro_check(core::sta_memory_pool::is_safe(),
                   "async_semaphore: destroyed on a thread other than its coroutines'",
                   detail::site::current());
        coro_check(waiters_.empty() && handed_ == 0,
                   "async_semaphore: destroyed while a coroutine waits on it",
                   detail::site::current());
    }

    /// Takes a permit if one is free, and never suspends. A permit handed to a waiter is not
    /// free: while anybody stands in line this answers false.
    /// \param where the caller's line, for a strict build's report; left to its default.
    inline bool try_acquire([[maybe_unused]] detail::site where = detail::site::current()) noexcept {
        coro_check(core::sta_memory_pool::is_safe(),
                   "async_semaphore: acquired on a thread other than its coroutines'", where);
        return take_free();
    }

    /// The wait for a permit, to co_await where it is made: a free permit is taken without
    /// suspending; otherwise the coroutine stands at the tail of the line until a release()
    /// hands it one, and is resumed on the loop's next turn. Under `token` the wait ends early
    /// with operation_canceled_exception, the permit untaken. A permit handed to a waiter whose
    /// token fires before its turn is passed on -- to the next in line, or back to the count --
    /// and the co_await ends with the cancellation, as an event wait does: nothing is lost.
    ///
    /// \param token what ends the wait, required: the wait may last as long as another coroutine
    ///        keeps its permit. A token of nobody is accepted and costs nothing.
    /// \param where the caller's line, for a strict build's report; left to its default.
    [[nodiscard]] inline acquire_awaiter acquire(
        cancellation_token token, [[maybe_unused]] detail::site where = detail::site::current()) {
        coro_check(core::sta_memory_pool::is_safe(),
                   "async_semaphore: acquired on a thread other than its coroutines'", where);
        return acquire_awaiter(*this, std::move(token));
    }

    /// Gives `n` permits back: each to the first in line, handed over with a turn of the loop,
    /// or, with nobody waiting, to the count. Resumes nobody here. The permits not held -- free
    /// and handed over -- may not exceed the maximum: a build that checks stops at a release
    /// over it, which is a permit given back twice.
    /// \param where the caller's line, for a strict build's report; left to its default.
    inline void release(std::ptrdiff_t n = 1,
                        [[maybe_unused]] detail::site where = detail::site::current()) noexcept {
        coro_check(core::sta_memory_pool::is_safe(),
                   "async_semaphore: released on a thread other than its coroutines'", where);
        // Subtracted rather than added: under the unbounded maximum the sum would overflow.
        coro_check(0 <= n && n <= max_ - count_ - handed_,
                   "async_semaphore: released over its maximum count", where);

        give(n);
    }

private:
    friend class detail::semaphore_wait;
    friend class async_mutex;

    /// A free permit, if there is one. One is free only while nobody stands in line: release()
    /// hands it to the first in line otherwise.
    inline bool take_free() noexcept {
        coro_check(count_ == 0 || waiters_.empty(),
                   "async_semaphore: a free permit beside a waiter in line",
                   detail::site::current());

        if (count_ == 0) return false;

        --count_;
        return true;
    }

    /// `n` permits to whoever comes first: the waiters in line, in their order, then the count.
    inline void give(std::ptrdiff_t n) noexcept {
        for (; n != 0 && !waiters_.empty(); --n) {
            detail::semaphore_wait* const first = waiters_.front();

            core::intrusive_list<detail::semaphore_wait>::remove(core::as_not_null(first));
            ++handed_;
            first->hand_over();
        }

        count_ += n;
    }

    /// A permit handed over whose waiter will not take it -- cancelled, or its frame gone before
    /// its turn: given on as a release() of it would give it.
    inline void pass_on_handed() noexcept {
        --handed_;
        give(1);
    }

    // The counts first: the list derives from core::noncopyable as the semaphore does, and two
    // such bases may not share an address, so the list first would cost a word of padding.

    /// The permits free: never more than zero while anybody stands in line.
    std::ptrdiff_t count_;

    /// The permits handed to waiters whose turn has not come yet: not free, and not held.
    std::ptrdiff_t handed_;

    /// The ceiling of the permits not held, free and handed over together.
    std::ptrdiff_t max_;

    /// The coroutines waiting, first to last.
    core::intrusive_list<detail::semaphore_wait> waiters_;
};

namespace detail {

inline bool semaphore_wait::await_ready() noexcept {
    if (stage_ == stage::canceled_before_suspend) [[unlikely]]
        return true;

    return semaphore_.take_free();
}

inline void semaphore_wait::await_suspend(std::coroutine_handle<> waiter) noexcept {
    waiter_ = waiter;
    semaphore_.waiters_.push_back(core::as_not_null(this));
}

inline void semaphore_wait::cancel() noexcept {
    if (linked()) {
        core::intrusive_list<semaphore_wait>::remove(core::as_not_null(this));
        stage_ = stage::canceled;
        sta_loop::queue_loop_turn(*this);
    } else if (stage_ == stage::handed) {
        // The turn already in line is the one that resumes it: never a second.
        stage_ = stage::canceled;
        semaphore_.pass_on_handed();
    } else {
        coro_check(stage_ == stage::idle && !waiter_,
                   "async_semaphore: a wait cancelled when it is neither waiting nor about to",
                   detail::site::current());
        stage_ = stage::canceled_before_suspend;
    }
}

inline void semaphore_wait::abandon() noexcept {
    if (linked()) {
        core::intrusive_list<semaphore_wait>::remove(core::as_not_null(this));
        return;
    }

    switch (stage_) {
    case stage::handed:
        // Out of line first, so that the permit passed on finds the line as it will stay.
        sta_loop::dequeue_loop_turn(*this);
        semaphore_.pass_on_handed();
        break;
    case stage::canceled:
        sta_loop::dequeue_loop_turn(*this);
        break;
    case stage::idle:
    case stage::canceled_before_suspend:
        break;
    }
}

inline bool semaphore_wait::take_turn(loop_turn& turn) {
    semaphore_wait& wait = static_cast<semaphore_wait&>(turn);

    // A second turn for one node would resume its coroutine twice.
    coro_check(wait.stage_ == stage::handed || wait.stage_ == stage::canceled,
               "async_semaphore: a turn for a wait that is owed none", detail::site::current());

    if (wait.stage_ == stage::handed) --wait.semaphore_.handed_;

    // Over before the waiter runs: whatever it does next, its frame owes nothing.
    wait.stage_ = stage::idle;
    wait.waiter_.resume();

    return true;
}

}  // namespace detail

}  // export namespace wxl::async
