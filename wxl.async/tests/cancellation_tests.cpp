// Asking a chain of coroutines to end, rather than destroying it: a token passed down the
// chain, a wait under it that ends with operation_canceled_exception, and the chain that
// unwinds through its own code -- where cleanup may co_await again -- and ends by itself.
// What has not ended when its owner can wait no longer is destroyed by the owner, the way
// it always was.
//
// The operations here are the module's own, on the loop: a read standing for one on a slow
// device, held at a gate on the worker, which cancelling opens -- the way CancelIoEx
// completes a read the kernel is holding.
#include <crtdbg.h>

#include <gtest/gtest.h>

#include "sta_pool.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

/// What the operations of one test saw and did, kept outside them: an operation is deleted
/// whenever its turn comes, and the test looks afterwards.
struct probe {
    /// Manual-reset: the bodies wait here, and whoever opens it opens it for good.
    hevent gate{true};

    /// Set by the first body to reach the gate, so a test can know the worker is inside it.
    hevent started{true};

    std::atomic<int> made{0};
    std::atomic<int> ran{0};
    std::atomic<int> wrote{0};
    std::atomic<int> told{0};

    /// Operations of this probe not yet deleted.
    std::atomic<int> alive{0};
};

/// The read: waits at the gate, then writes into the caller's buffer -- in the frame --
/// unless it was asked to stop meanwhile, in which case it comes back cut short, with
/// nothing written, as an overlapped read cancelled by CancelIoEx does.
class gated_read : public async_op_t<std::size_t>
{
public:
    gated_read(probe& p, std::span<std::byte> into) : p_(p), into_(into) {
        ++p_.made;
        ++p_.alive;
    }

    ~gated_read() override { --p_.alive; }

protected:
    bool execute() override {
        p_.started.set();
        p_.gate.wait();

        ++p_.ran;

        if (canceled()) {
            set_error(std::make_exception_ptr(std::runtime_error("cut short")));
            return true;
        }

        std::ranges::fill(into_, std::byte{42});
        ++p_.wrote;

        set_value(into_.size());
        return true;
    }

    void on_cancel() noexcept override {
        ++p_.told;
        p_.gate.set();
    }

private:
    probe& p_;
    std::span<std::byte> into_;
};

awaitable<std::size_t> start_read(probe& p, std::span<std::byte> into) {
    return sta_loop::async_run(std::unique_ptr<async_op_t<std::size_t>>(new gated_read(p, into)));
}

/// The same read under a token, in the form the operations of this module take one:
/// `async_file::read_all(path, stop)`.
cancellable_awaitable<std::size_t> start_read(probe& p, std::span<std::byte> into, cancellation_token stop) {
    return cancellable_awaitable<std::size_t>(std::move(stop), [&] { return start_read(p, into); });
}

/// Three links, the token passed down by hand; the bottom one reads into its frame.
task<std::size_t> bottom(probe& p, cancellation_token stop) {
    std::byte buf[16]{};
    co_return co_await cancellable(start_read(p, buf), stop);
}

task<std::size_t> middle(probe& p, cancellation_token stop) {
    co_return co_await bottom(p, stop);
}

task<std::size_t> top(probe& p, cancellation_token stop) {
    co_return co_await middle(p, stop);
}

/// Undoes what the chain did: an operation of its own, under no token.
task<> roll_back(probe& undo, std::vector<std::string>& log) {
    std::byte buf[8]{};
    co_await start_read(undo, buf);
    log.push_back("rolled back");
}

/// Catches the cancellation, cleans up with a co_await -- after the handler, since C++
/// allows no co_await inside one -- and passes it on.
task<std::size_t> middle_that_rolls_back(probe& p, probe& undo, cancellation_token stop,
                                         std::vector<std::string>& log) {
    std::exception_ptr failure;

    try {
        co_return co_await bottom(p, stop);
    } catch (const operation_canceled_exception&) {
        log.push_back("told");
        failure = std::current_exception();
    }

    co_await roll_back(undo, log);
    std::rethrow_exception(failure);
}

/// Was given the token and does not use it: its read cannot be interrupted.
task<std::size_t> deaf(probe& p, cancellation_token) {
    std::byte buf[16]{};
    const std::size_t got = co_await start_read(p, buf);

    ADD_FAILURE() << "resumed after its chain was swept";
    co_return got;
}

/// An orphan that was not given the token, and one that asked for it.
detached_task orphan(probe& p, std::size_t& got, bool& ended) {
    std::byte buf[16]{};
    got = co_await start_read(p, buf);
    ended = true;
}

detached_task orphan_that_asked(probe& p, cancellation_token stop, bool& canceled) {
    std::byte buf[16]{};

    try {
        co_await cancellable(start_read(p, buf), stop);
    } catch (const operation_canceled_exception&) {
        canceled = true;
    }
}

/// The chain's own read goes out first, then the orphans': the worker takes them in turn.
task<std::size_t> spawns_two_orphans(probe& own, probe& unasked, probe& asking,
                                     cancellation_token stop, std::size_t& unasked_got,
                                     bool& unasked_ended, bool& asking_canceled) {
    task<std::size_t> own_read = bottom(own, stop);

    orphan_that_asked(asking, stop, asking_canceled);
    orphan(unasked, unasked_got, unasked_ended);

    co_return co_await own_read;
}

/// Lets a test resume a coroutine by hand. It has no cancel(): a wait standing on it
/// cannot be told.
struct parked {
    std::coroutine_handle<>& slot;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> here) noexcept { slot = here; }
    int await_resume() const noexcept { return 7; }
};

task<int> waits_where_nobody_can_tell(std::coroutine_handle<>& slot, cancellation_token stop) {
    co_return co_await cancellable(parked{slot}, stop);
}

/// An awaiter that does not move, as the waits of wxl.ui do not; it can be told.
class pinned
{
public:
    explicit pinned(std::coroutine_handle<>& slot, int& told) noexcept : slot_(slot), told_(told) {}

    pinned(const pinned&) = delete;
    pinned& operator=(const pinned&) = delete;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> here) noexcept { slot_ = here; }
    int await_resume() const noexcept { return 3; }

    void cancel() noexcept { ++told_; }

private:
    std::coroutine_handle<>& slot_;
    int& told_;
};

task<int> waits_on_a_pinned_awaiter(std::coroutine_handle<>& slot, int& told, cancellation_token stop) {
    co_return co_await cancellable(pinned{slot, told}, stop);
}

/// Three links again, the bottom one reading by the form with a token.
task<std::size_t> overload_bottom(probe& p, cancellation_token stop) {
    std::byte buf[16]{};
    co_return co_await start_read(p, buf, stop);
}

task<std::size_t> overload_top(probe& p, cancellation_token stop) {
    co_return co_await overload_bottom(p, stop);
}

/// Starts the read under the token, keeps it, waits for something else first, and only
/// then awaits the read.
task<std::size_t> keeps_the_read(probe& p, cancellation_token stop, std::coroutine_handle<>& slot) {
    std::byte buf[16]{};
    cancellable_awaitable<std::size_t> read = start_read(p, buf, std::move(stop));

    co_await parked{slot};
    co_return co_await read;
}

/// Moves the read into a container before awaiting it there.
task<std::size_t> moves_the_read(probe& p, cancellation_token stop) {
    std::byte buf[16]{};
    std::vector<cancellable_awaitable<std::size_t>> reads;

    reads.push_back(start_read(p, buf, std::move(stop)));

    co_return co_await reads.front();
}

/// What cancelled() answered.
template <class Task>
bool ends_cancelled(Task& t) {
    try {
        (void)t.result();
    } catch (const operation_canceled_exception&) {
        return true;
    } catch (...) {
    }

    return false;
}

}  // namespace

// The token reaches the read at the bottom of three links: asking interrupts it, and
// nothing more -- nobody is resumed from inside cancel(). The read comes back through the
// loop cut short, the bottom link's wait ends with the cancellation, and the exception
// climbs to the top.
TEST(CancellationTest, TheRequestReachesTheOperationAtTheBottomOfAChain) {
    probe p;
    cancellation_source stop;
    task<std::size_t> chain = top(p, stop.token());

    p.started.wait();
    EXPECT_FALSE(chain.done());

    stop.cancel();

    EXPECT_EQ(p.told, 1);
    EXPECT_FALSE(chain.done()) << "cancel() resumed the chain itself";

    sta_loop::run_until([&] { return chain.done(); });

    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(p.wrote, 0);
    EXPECT_EQ(p.alive, 0);
}

// The middle link catches the cancellation and cleans up with an operation of its own,
// awaited like any other; then the chain ends by itself, and its owner destroys nothing
// that is still standing.
TEST(CancellationTest, AChainCleansUpAfterItselfWithACoAwaitAndEndsByItself) {
    probe p, undo;
    undo.gate.set();

    std::vector<std::string> log;
    cancellation_source stop;
    task<std::size_t> chain = middle_that_rolls_back(p, undo, stop.token(), log);

    p.started.wait();
    stop.cancel();

    sta_loop::run_until([&] { return chain.done(); });

    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(log, (std::vector<std::string>{"told", "rolled back"}));
    EXPECT_EQ(undo.ran, 1);
    EXPECT_EQ(undo.wrote, 1);
    EXPECT_EQ(undo.told, 0) << "the cleanup ran under the token";
    EXPECT_EQ(p.alive + undo.alive, 0);
}

// Two chains told at once: one listens and ends, the other stands on a read it was never
// asked to cut short. The sweep finds the first ended, and destroys the second, whose read
// is given up the usual way.
TEST(CancellationTest, SweepingDestroysOnlyWhatHasNotEnded) {
    probe listens, does_not;
    cancellation_source stop;

    std::vector<task<std::size_t>> owned;
    owned.push_back(top(listens, stop.token()));
    owned.push_back(deaf(does_not, stop.token()));

    // One worker: the second read is reached once the first has come back.
    listens.started.wait();
    stop.cancel();

    sta_loop::run_until([&] { return owned[0].done(); });

    does_not.started.wait();
    EXPECT_FALSE(owned[1].done());
    EXPECT_EQ(listens.told, 1);
    EXPECT_EQ(does_not.told, 0);

    // The second step: what ended is read and let go of; what still stands is destroyed.
    std::erase_if(owned, [](task<std::size_t>& t) {
        if (!t.done()) return false;
        EXPECT_TRUE(ends_cancelled(t));
        return true;
    });

    ASSERT_EQ(owned.size(), 1u);
    owned.clear();

    EXPECT_EQ(does_not.told, 1) << "giving the read up asks it to stop";
    EXPECT_EQ(does_not.wrote, 0);
    EXPECT_EQ(listens.alive + does_not.alive, 0);
}

// Asked before it began: the read the bottom link starts is told at once, and since the
// worker has not reached it, it never runs. The wait still lasts until the read has come
// back -- without holding the thread -- and only then ends with the cancellation.
TEST(CancellationTest, ACancellationBeforeTheFirstWaitEndsTheChainThere) {
    probe ahead, p;
    std::byte ahead_buf[16]{};

    // Keeps the worker busy, so that the chain's read waits in the queue behind it.
    auto holds_the_worker = start_read(ahead, ahead_buf);
    ahead.started.wait();

    cancellation_source stop;
    stop.cancel();

    task<std::size_t> chain = top(p, stop.token());

    EXPECT_EQ(p.told, 1);
    EXPECT_FALSE(chain.done());

    ahead.gate.set();
    sta_loop::run_until([&] { return chain.done() && holds_the_worker.ready(); });

    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(p.ran, 0) << "a read told before the worker reached it was started";
    EXPECT_EQ(p.alive, 0);
}

// A chain that has ended is not reached: its wait left the token's list on the way out.
// Asking twice is asking once.
TEST(CancellationTest, CancellingAChainThatHasEndedDoesNothing) {
    probe p;
    p.gate.set();

    cancellation_source stop;
    task<std::size_t> chain = top(p, stop.token());

    sta_loop::run_until([&] { return chain.done(); });

    stop.cancel();
    stop.cancel();

    EXPECT_EQ(p.told, 0);
    EXPECT_EQ(chain.result(), 16u);
    EXPECT_TRUE(stop.is_canceled());
}

// An orphan started from the chain is not the chain's: without the token it reads on and
// ends with its value, while the one that took the token is told with the chain.
TEST(CancellationTest, AnOrphanIsNotToldUnlessItTookTheToken) {
    probe own, unasked, asking;
    std::size_t unasked_got = 0;
    bool unasked_ended = false, asking_canceled = false;

    cancellation_source stop;
    task<std::size_t> chain = spawns_two_orphans(own, unasked, asking, stop.token(), unasked_got,
                                                 unasked_ended, asking_canceled);

    own.started.wait();
    stop.cancel();

    EXPECT_EQ(own.told, 1);
    EXPECT_EQ(asking.told, 1);
    EXPECT_EQ(unasked.told, 0);

    sta_loop::run_until([&] { return chain.done() && asking_canceled; });

    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(asking.wrote, 0);

    unasked.started.wait();
    EXPECT_FALSE(unasked_ended);

    unasked.gate.set();
    sta_loop::run_until([&] { return unasked_ended; });

    EXPECT_EQ(unasked_got, 16u);
    EXPECT_EQ(unasked.wrote, 1);
}

// A frame destroyed while it stands under the token takes its wait off the token's list:
// asking afterwards reaches nothing that is gone.
TEST(CancellationTest, AFrameDestroyedWhileWaitingLeavesTheToken) {
    probe p;
    cancellation_source stop;
    {
        task<std::size_t> chain = top(p, stop.token());
        p.started.wait();
    }

    EXPECT_EQ(p.told, 1) << "the read was given up";

    stop.cancel();

    EXPECT_EQ(p.told, 1);
    EXPECT_EQ(p.alive, 0);
}

// A wait on something that cannot be told is not interrupted: it ends when it ends, and
// then with the cancellation. One that would begin after the request does not stand at all.
TEST(CancellationTest, AWaitThatCannotBeToldEndsWhenItEndsAndThenWithTheCancellation) {
    std::coroutine_handle<> slot;
    cancellation_source stop;
    task<int> standing = waits_where_nobody_can_tell(slot, stop.token());

    ASSERT_TRUE(slot);
    stop.cancel();
    EXPECT_FALSE(standing.done());

    slot.resume();
    EXPECT_TRUE(ends_cancelled(standing));

    std::coroutine_handle<> unused;
    task<int> late = waits_where_nobody_can_tell(unused, stop.token());

    EXPECT_TRUE(late.done());
    EXPECT_FALSE(unused) << "a wait began standing after the request";
    EXPECT_TRUE(ends_cancelled(late));
}

// An awaiter that does not move is borrowed for the full expression it was made in -- the
// co_await's -- and told like any other.
TEST(CancellationTest, AnAwaiterThatDoesNotMoveIsBorrowedForItsCoAwait) {
    std::coroutine_handle<> slot;
    int told = 0;
    cancellation_source stop;

    task<int> plain = waits_on_a_pinned_awaiter(slot, told, cancellation_token{});
    ASSERT_TRUE(slot);
    slot.resume();
    EXPECT_EQ(plain.result(), 3);

    task<int> asked = waits_on_a_pinned_awaiter(slot, told, stop.token());
    stop.cancel();
    EXPECT_EQ(told, 1);

    slot.resume();
    EXPECT_TRUE(ends_cancelled(asked));
}

// Without a request the wait is the operand's own, and the read is never told.
TEST(CancellationTest, WithoutARequestTheWaitIsTheOperandsOwn) {
    std::coroutine_handle<> slot;
    cancellation_source stop;
    task<int> waits = waits_where_nobody_can_tell(slot, stop.token());

    ASSERT_TRUE(slot);
    slot.resume();

    EXPECT_EQ(waits.result(), 7);

    // A token that is nobody's is never cancelled, and has no state behind it.
    task<int> unowned = waits_where_nobody_can_tell(slot, cancellation_token{});
    slot.resume();

    EXPECT_EQ(unowned.result(), 7);
}

// ---- The operations' own form under a token --------------------------------------------

// Without a request the form with a token answers what the plain one does, and its
// operation is never told.
TEST(CancellationTest, TheFormWithATokenAnswersLikeThePlainOne) {
    probe p;
    p.gate.set();

    cancellation_source stop;
    task<std::size_t> chain = overload_top(p, stop.token());

    sta_loop::run_until([&] { return chain.done(); });

    EXPECT_EQ(chain.result(), 16u);
    EXPECT_EQ(p.told, 0);
    EXPECT_EQ(p.wrote, 1);
}

TEST(CancellationTest, TheFormWithATokenIsToldAtTheBottomOfAChain) {
    probe p;
    cancellation_source stop;
    task<std::size_t> chain = overload_top(p, stop.token());

    p.started.wait();
    stop.cancel();

    EXPECT_EQ(p.told, 1);
    EXPECT_FALSE(chain.done());

    sta_loop::run_until([&] { return chain.done(); });

    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(p.wrote, 0);
    EXPECT_EQ(p.alive, 0);
}

// Under a token cancelled already the operation is not even made: the co_await ends at
// once, without a trip to the worker.
TEST(CancellationTest, UnderACancelledTokenTheFormWithATokenStartsNothing) {
    probe p;
    cancellation_source stop;
    stop.cancel();

    task<std::size_t> chain = overload_top(p, stop.token());

    EXPECT_TRUE(chain.done());
    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(p.made, 0);
}

// What the form with a token returns is an object like the awaitable: kept and awaited
// later, it holds its token, so it does not matter what has become of the source by then.
TEST(CancellationTest, AKeptOperationUnderATokenIsAwaitedLater) {
    std::coroutine_handle<> slot;

    {
        probe p;
        p.gate.set();

        std::optional<cancellation_source> stop(std::in_place);
        task<std::size_t> chain = keeps_the_read(p, stop->token(), slot);

        stop.reset();

        ASSERT_TRUE(slot);
        slot.resume();
        sta_loop::run_until([&] { return chain.done(); });

        EXPECT_EQ(chain.result(), 16u);
        EXPECT_EQ(p.told, 0);
    }

    // Asked while it is kept, it is not standing yet and nobody is told; the co_await
    // tells it, waits for it to come back, and ends with the cancellation.
    probe p;
    cancellation_source stop;
    task<std::size_t> chain = keeps_the_read(p, stop.token(), slot);

    p.started.wait();
    stop.cancel();
    EXPECT_EQ(p.told, 0);

    slot.resume();
    EXPECT_EQ(p.told, 1);

    sta_loop::run_until([&] { return chain.done(); });

    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(p.wrote, 0);
    EXPECT_EQ(p.alive, 0);
}

// And it moves, while it is not awaited.
TEST(CancellationTest, AnOperationUnderATokenMovesBeforeItIsAwaited) {
    {
        probe p;
        p.gate.set();

        cancellation_source stop;
        task<std::size_t> chain = moves_the_read(p, stop.token());

        sta_loop::run_until([&] { return chain.done(); });
        EXPECT_EQ(chain.result(), 16u);
    }

    probe p;
    cancellation_source stop;
    task<std::size_t> chain = moves_the_read(p, stop.token());

    p.started.wait();
    stop.cancel();
    EXPECT_EQ(p.told, 1);

    sta_loop::run_until([&] { return chain.done(); });
    EXPECT_TRUE(ends_cancelled(chain));
}

// A frame destroyed while it stands on the form with a token takes the wait off the list
// before the token it holds goes: asking afterwards reaches nothing that is gone, and a
// token that was the state's last holder lets go of it only once the wait is off.
TEST(CancellationTest, AFrameDestroyedOnTheFormWithATokenLeavesTheToken) {
    {
        probe p;
        cancellation_source stop;
        {
            task<std::size_t> chain = overload_top(p, stop.token());
            p.started.wait();
        }

        EXPECT_EQ(p.told, 1) << "the read was given up";

        stop.cancel();
        EXPECT_EQ(p.told, 1);
    }

    probe p;
    std::coroutine_handle<> slot;
    std::optional<cancellation_source> stop(std::in_place);
    {
        task<std::size_t> chain = keeps_the_read(p, stop->token(), slot);

        p.started.wait();
        slot.resume();

        // The kept read now stands on the list of a state only its own token holds.
        stop.reset();
    }

    EXPECT_EQ(p.told, 1);
    EXPECT_EQ(p.alive, 0);
}

// The token is one thread's: a build that checks coroutines stops a cancel() from any other
// -- assert in a Debug build, core::abort with the caller's line under STRICT_CORO.

namespace {

/// A failed check goes to stderr and ends the process without a dialog, so
/// that a death test can read why.
void report_failures_to_stderr() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

constexpr std::uint_least32_t line_of_the_foreign_cancel = std::source_location::current().line() + 1;
void cancel_there(cancellation_source& stop) { stop.cancel(); }

void cancel_from_another_thread() {
    report_failures_to_stderr();

    cancellation_source stop;
    std::thread([&stop] { cancel_there(stop); }).join();
}

class CancellationDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!coro_detail::checked) GTEST_SKIP() << "built without coroutine checks";
    }
};

/// A strict build's report names the line of the caller.
class CancellationStrictDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!coro_detail::strict) GTEST_SKIP() << "built without STRICT_CORO";
    }
};

}  // namespace

TEST_F(CancellationDeathTest, ASourceIsCancelledOnItsCoroutinesThread) {
    EXPECT_DEATH(cancel_from_another_thread(), "cancelled from a thread other than its coroutines'");
}

TEST_F(CancellationStrictDeathTest, TheReportNamesTheLineOfTheForeignCancel) {
    EXPECT_DEATH(cancel_from_another_thread(),
                 std::format("cancellation_tests\\.cpp\\({}\\): cancellation_source: cancelled from a thread",
                             line_of_the_foreign_cancel));
}

static_assert(sizeof(cancellation_token) == sizeof(void*));
static_assert(cancellable_awaiter<awaitable<std::size_t>>);
static_assert(!cancellable_awaiter<parked>);

// The form with a token adds the token and the place in its list to the awaitable, and
// nothing to the plain form.
static_assert(sizeof(awaitable<std::size_t>) == sizeof(void*));
static_assert(sizeof(cancellable_awaitable<std::size_t>) == 5 * sizeof(void*));

