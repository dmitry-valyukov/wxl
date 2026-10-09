// Asking a chain of coroutines to end, rather than destroying it: a token passed down the
// chain, a wait under it that ends with operation_canceled_exception, and the chain that
// unwinds through its own code -- where cleanup may co_await again -- and ends by itself.
// What has not ended when its owner can wait no longer is destroyed by the owner, the way
// it always was.
//
// The operations here are the module's own, on the loop: a read standing for one on a slow
// device, held at a gate on the worker, which cancelling opens -- the way CancelIoEx
// completes a read the kernel is holding.
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
    gated_read(probe& p, std::span<std::byte> into) : p_(p), into_(into) { ++p_.alive; }

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

static_assert(sizeof(cancellation_token) == sizeof(void*));
static_assert(cancellable_awaiter<awaitable<std::size_t>>);
static_assert(!cancellable_awaiter<parked>);

// The implicit token, built with WXL_AMBIENT_CANCELLATION: the same chains with no token in
// their signatures. A task takes the token of the coroutine that calls it, at birth; a
// detached task takes none unless it names one; cleanup that must not be cut short stands
// under the shield, a scope with no token.
#ifdef WXL_AMBIENT_CANCELLATION

namespace {

/// Calls `start` under `token`, as the owner of a chain does in code that is no
/// coroutine's.
template <class Start>
auto started_under(const cancellation_token& token, Start start) {
    cancellation_scope under(token);
    return start();
}

task<std::size_t> implicit_bottom(probe& p) {
    std::byte buf[16]{};
    co_return co_await start_read(p, buf);
}

task<std::size_t> implicit_middle(probe& p) {
    co_return co_await implicit_bottom(p);
}

task<std::size_t> implicit_top(probe& p) {
    co_return co_await implicit_middle(p);
}

task<std::size_t> implicit_middle_that_rolls_back(probe& p, probe& undo, std::vector<std::string>& log) {
    std::exception_ptr failure;

    try {
        co_return co_await implicit_bottom(p);
    } catch (const operation_canceled_exception&) {
        log.push_back("told");
        failure = std::current_exception();
    }

    {
        cancellation_scope shield;
        co_await roll_back(undo, log);
    }

    std::rethrow_exception(failure);
}

/// Stands on its read under the shield: the request does not reach it.
task<std::size_t> shielded(probe& p) {
    cancellation_scope shield;

    std::byte buf[16]{};
    const std::size_t got = co_await start_read(p, buf);

    ADD_FAILURE() << "resumed after its chain was swept";
    co_return got;
}

detached_task implicit_orphan(probe& p, std::size_t& got, bool& ended) {
    std::byte buf[16]{};
    got = co_await start_read(p, buf);
    ended = true;
}

detached_task implicit_orphan_that_asked(probe& p, cancellation_token stop, bool& canceled) {
    cancellation_scope under(stop);
    std::byte buf[16]{};

    try {
        co_await start_read(p, buf);
    } catch (const operation_canceled_exception&) {
        canceled = true;
    }
}

task<std::size_t> implicit_spawns_two_orphans(probe& own, probe& unasked, probe& asking,
                                              cancellation_token stop, std::size_t& unasked_got,
                                              bool& unasked_ended, bool& asking_canceled) {
    task<std::size_t> own_read = implicit_bottom(own);

    implicit_orphan_that_asked(asking, stop, asking_canceled);
    implicit_orphan(unasked, unasked_got, unasked_ended);

    co_return co_await own_read;
}

/// Catches the cancellation and answers all the same: its own outcome is what the task
/// that awaits it gets.
task<std::size_t> implicit_falls_back(probe& p) {
    try {
        co_return co_await implicit_bottom(p);
    } catch (const operation_canceled_exception&) {
    }

    co_return 5;
}

task<std::size_t> implicit_awaits_a_fallback(probe& p) {
    co_return co_await implicit_falls_back(p) + 1;
}

/// Waits by hand, then starts a read: whose token the read stands under says which frame
/// was running when it was born.
task<std::size_t> parks_then_reads(std::coroutine_handle<>& slot, probe& p) {
    co_await parked{slot};
    co_return co_await implicit_bottom(p);
}

/// Resumes another coroutine from inside its own body, as an event handler does, and starts
/// a read once that one has suspended again.
task<std::size_t> resumes_another_then_reads(std::coroutine_handle<>& own, std::coroutine_handle<>& other,
                                             probe& p) {
    co_await parked{own};
    other.resume();
    co_return co_await implicit_bottom(p);
}

task<int> answers_at_once() {
    co_return 7;
}

/// Awaits a task that has already ended -- no suspension -- before suspending for real.
task<std::size_t> awaits_one_at_once_then_reads(std::coroutine_handle<>& slot, probe& p) {
    std::size_t got = static_cast<std::size_t>(co_await answers_at_once());
    co_await parked{slot};
    got += co_await implicit_bottom(p);
    co_return got;
}

}  // namespace

TEST(ImplicitCancellationTest, TheRequestReachesTheOperationAtTheBottomOfAChain) {
    probe p;
    cancellation_source stop;
    task<std::size_t> chain = started_under(stop.token(), [&] { return implicit_top(p); });

    p.started.wait();
    stop.cancel();

    EXPECT_EQ(p.told, 1);
    EXPECT_FALSE(chain.done());

    sta_loop::run_until([&] { return chain.done(); });

    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(p.wrote, 0);
    EXPECT_EQ(p.alive, 0);
}

TEST(ImplicitCancellationTest, AChainCleansUpUnderTheShieldAndEndsByItself) {
    probe p, undo;
    undo.gate.set();

    std::vector<std::string> log;
    cancellation_source stop;
    task<std::size_t> chain =
        started_under(stop.token(), [&] { return implicit_middle_that_rolls_back(p, undo, log); });

    p.started.wait();
    stop.cancel();

    sta_loop::run_until([&] { return chain.done(); });

    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(log, (std::vector<std::string>{"told", "rolled back"}));
    EXPECT_EQ(undo.wrote, 1);
    EXPECT_EQ(undo.told, 0) << "the cleanup ran under the token";
}

TEST(ImplicitCancellationTest, SweepingDestroysOnlyWhatHasNotEnded) {
    probe listens, does_not;
    cancellation_source stop;

    std::vector<task<std::size_t>> owned;
    {
        cancellation_scope under(stop.token());
        owned.push_back(implicit_top(listens));
        owned.push_back(shielded(does_not));
    }

    listens.started.wait();
    stop.cancel();

    sta_loop::run_until([&] { return owned[0].done(); });

    does_not.started.wait();
    EXPECT_FALSE(owned[1].done());
    EXPECT_EQ(does_not.told, 0);

    std::erase_if(owned, [](task<std::size_t>& t) { return t.done() && ends_cancelled(t); });
    ASSERT_EQ(owned.size(), 1u);
    owned.clear();

    EXPECT_EQ(does_not.told, 1);
    EXPECT_EQ(does_not.wrote, 0);
    EXPECT_EQ(listens.alive + does_not.alive, 0);
}

TEST(ImplicitCancellationTest, ACancellationBeforeTheFirstWaitEndsTheChainThere) {
    probe ahead, p;
    std::byte ahead_buf[16]{};

    auto holds_the_worker = start_read(ahead, ahead_buf);
    ahead.started.wait();

    cancellation_source stop;
    stop.cancel();

    task<std::size_t> chain = started_under(stop.token(), [&] { return implicit_top(p); });

    EXPECT_EQ(p.told, 1);
    EXPECT_FALSE(chain.done());

    ahead.gate.set();
    sta_loop::run_until([&] { return chain.done() && holds_the_worker.ready(); });

    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(p.ran, 0);
}

TEST(ImplicitCancellationTest, CancellingAChainThatHasEndedDoesNothing) {
    probe p;
    p.gate.set();

    cancellation_source stop;
    task<std::size_t> chain = started_under(stop.token(), [&] { return implicit_top(p); });

    sta_loop::run_until([&] { return chain.done(); });

    stop.cancel();

    EXPECT_EQ(p.told, 0);
    EXPECT_EQ(chain.result(), 16u);
}

TEST(ImplicitCancellationTest, AnOrphanInheritsNothingUnlessItNamesAToken) {
    probe own, unasked, asking;
    std::size_t unasked_got = 0;
    bool unasked_ended = false, asking_canceled = false;

    cancellation_source stop;
    task<std::size_t> chain = started_under(stop.token(), [&] {
        return implicit_spawns_two_orphans(own, unasked, asking, stop.token(), unasked_got, unasked_ended,
                                           asking_canceled);
    });

    own.started.wait();
    stop.cancel();

    EXPECT_EQ(own.told, 1);
    EXPECT_EQ(asking.told, 1);
    EXPECT_EQ(unasked.told, 0);

    sta_loop::run_until([&] { return chain.done() && asking_canceled; });
    EXPECT_TRUE(ends_cancelled(chain));

    unasked.started.wait();
    unasked.gate.set();
    sta_loop::run_until([&] { return unasked_ended; });

    EXPECT_EQ(unasked_got, 16u);
}

// A wait for another task is not answered for it: that task stands under the same token
// and answers for itself -- here with a value.
TEST(ImplicitCancellationTest, AWaitForATaskTakesWhatTheTaskAnswers) {
    probe p;
    cancellation_source stop;
    task<std::size_t> chain = started_under(stop.token(), [&] { return implicit_awaits_a_fallback(p); });

    p.started.wait();
    stop.cancel();

    sta_loop::run_until([&] { return chain.done(); });
    EXPECT_EQ(chain.result(), 6u);
}

// The frame that runs is handed back where a body suspends: once a chain started under a
// token has suspended, code that is no coroutine's is under no token again, and what it
// starts is not told.
TEST(ImplicitCancellationTest, ASuspendedChainLeavesTheCodeAroundItUnderNoToken) {
    probe told, untold;
    told.gate.set();

    std::coroutine_handle<> slot;
    cancellation_source stop;
    task<std::size_t> chain = started_under(stop.token(), [&] { return parks_then_reads(slot, told); });

    ASSERT_TRUE(slot);

    task<std::size_t> outside = implicit_bottom(untold);
    untold.started.wait();

    stop.cancel();
    EXPECT_EQ(untold.told, 0);

    untold.gate.set();
    sta_loop::run_until([&] { return outside.done(); });
    EXPECT_EQ(outside.result(), 16u);

    // The chain itself, resumed, starts its read under its own token, which is cancelled.
    slot.resume();
    sta_loop::run_until([&] { return chain.done(); });
    EXPECT_TRUE(ends_cancelled(chain));
}

// A body that resumes another coroutine from inside itself, as an event handler does, has
// the thread back once that one suspends: what it starts afterwards is under its own token.
TEST(ImplicitCancellationTest, ABodyThatResumesAnotherIsRunningAgainWhenThatOneSuspends) {
    probe mine, theirs;
    theirs.gate.set();

    std::coroutine_handle<> own, other;
    cancellation_source stop;

    task<std::size_t> resumer = started_under(stop.token(), [&] { return resumes_another_then_reads(own, other, mine); });
    task<std::size_t> resumed = parks_then_reads(other, theirs);

    ASSERT_TRUE(own && other);

    own.resume();          // resumes `other`, which starts its read under no token and suspends
    mine.started.wait();   // the worker takes `mine` once `theirs` is back

    stop.cancel();

    EXPECT_EQ(mine.told, 1);
    EXPECT_EQ(theirs.told, 0);

    sta_loop::run_until([&] { return resumer.done() && resumed.done(); });

    EXPECT_TRUE(ends_cancelled(resumer));
    EXPECT_EQ(resumed.result(), 16u);
}

// A wait that does not suspend leaves the frame where it was: the one after it hands the
// thread back to the right place.
TEST(ImplicitCancellationTest, AWaitThatDoesNotSuspendLeavesTheRunningFrameAlone) {
    probe p, untold;
    untold.gate.set();
    p.gate.set();

    std::coroutine_handle<> slot;
    cancellation_source stop;
    task<std::size_t> chain = started_under(stop.token(), [&] { return awaits_one_at_once_then_reads(slot, p); });

    ASSERT_TRUE(slot);

    // Out here, under no token: a cancel does not reach it.
    task<std::size_t> outside = implicit_bottom(untold);
    stop.cancel();

    sta_loop::run_until([&] { return outside.done(); });
    EXPECT_EQ(outside.result(), 16u);
    EXPECT_EQ(untold.told, 0);

    slot.resume();
    sta_loop::run_until([&] { return chain.done(); });
    EXPECT_TRUE(ends_cancelled(chain));
}

// What every frame pays for the implicit token: the token it stands under and the frame
// that resumed it -- two words in the promise of every task and every detached task.
#ifdef NDEBUG
static_assert(sizeof(task_detail::promise_base) ==
              2 * sizeof(void*) + sizeof(std::coroutine_handle<>) + sizeof(std::exception_ptr));
#endif
static_assert(sizeof(detached_task::promise_type) == 3 * sizeof(void*));

#endif
