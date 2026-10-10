// Asking a chain of coroutines to end, rather than destroying it: a token passed down the
// chain, an operation of this module made in its form with a token and standing under it,
// a co_await that ends with operation_canceled_exception, and the chain that unwinds
// through its own code -- where cleanup may co_await again -- and ends by itself. What has
// not ended when its owner can wait no longer is destroyed by the owner, the way it always
// was. What wxl does not make, the application's own coroutine asks its token about itself.
//
// The operations here are the module's own, on the loop: a read standing for one on a slow
// device, held at a gate on the worker, which cancelling opens -- the way CancelIoEx
// completes a read the kernel is holding.
//
// What a co_await ends with is what happened to the operation: the cancellation if it was
// cut short or never started, and its own answer -- its value or its own failure -- if it
// got there first, request or no request.
#include <crtdbg.h>

#include <gtest/gtest.h>

#include "platform.h"

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
/// nothing written and the cancellation for its answer, as an overlapped read cancelled by
/// CancelIoEx does.
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
            set_error(std::make_exception_ptr(operation_canceled_exception()));
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

task<std::size_t> start_read(probe& p, std::span<std::byte> into) {
    return sta_loop::async_run(std::unique_ptr<async_op_t<std::size_t>>(new gated_read(p, into)));
}

/// The same read under a token, in the form the operations of this module take one --
/// `async_file::read_all(path, stop)` -- and the same task.
task<std::size_t> start_read(probe& p, std::span<std::byte> into, cancellation_token stop) {
    return cancellation_detail::run_under<gated_read>(std::move(stop), p, into);
}

/// Three links, the token passed down by hand; the bottom one reads into its frame.
task<std::size_t> bottom(probe& p, cancellation_token stop) {
    std::byte buf[16]{};
    co_return co_await start_read(p, buf, stop);
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

/// Joins a read somebody else keeps.
task<std::size_t> joins(task<std::size_t>& read) {
    co_return co_await read;
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
        co_await start_read(p, buf, stop);
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

/// Lets a test resume a coroutine by hand: an awaiter of the application's own, which wxl
/// does not reach.
struct parked {
    std::coroutine_handle<>& slot;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> here) noexcept { slot = here; }
    int await_resume() const noexcept { return 7; }
};

/// Waits on its own awaiter, and asks its token itself once the wait is over: what the
/// application does where wxl has no form with a token.
task<int> asks_its_token_itself(std::coroutine_handle<>& slot, cancellation_token stop) {
    const int got = co_await parked{slot};
    stop.throw_if_canceled();
    co_return got;
}

/// Starts the read under the token, keeps it, waits for something else first, and only
/// then awaits the read.
task<std::size_t> keeps_the_read(probe& p, cancellation_token stop, std::coroutine_handle<>& slot) {
    std::byte buf[16]{};
    task<std::size_t> read = start_read(p, buf, std::move(stop));

    co_await parked{slot};
    co_return co_await read;
}

/// Moves the read into a container before awaiting it there.
task<std::size_t> moves_the_read(probe& p, cancellation_token stop) {
    std::byte buf[16]{};
    std::vector<task<std::size_t>> reads;

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

// Asked after the read was made and before the worker reached it: the read is told at
// once and never runs. The co_await still lasts until the read has come back -- without
// holding the thread -- and only then ends with the cancellation.
TEST(CancellationTest, ARequestBeforeTheWorkerReachesTheReadKeepsItFromRunning) {
    probe ahead, p;
    std::byte ahead_buf[16]{};

    // Keeps the worker busy, so that the chain's read waits in the queue behind it.
    auto holds_the_worker = start_read(ahead, ahead_buf);
    ahead.started.wait();

    cancellation_source stop;
    task<std::size_t> chain = top(p, stop.token());

    stop.cancel();

    EXPECT_EQ(p.made, 1);
    EXPECT_EQ(p.told, 1);
    EXPECT_FALSE(chain.done());

    ahead.gate.set();
    sta_loop::run_until([&] { return chain.done() && holds_the_worker.done(); });

    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(p.ran, 0) << "a read told before the worker reached it was started";
    EXPECT_EQ(p.alive, 0);
}

// A chain that has ended is not reached: its read left the token's event when its task went.
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

// A frame destroyed while its read stands under the token gives the read up, and the read
// leaves the token's event as it goes: asking afterwards reaches nothing that is gone.
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

// What wxl does not make, it does not reach: the application's own awaiter is not under the
// token, and the coroutine asks the token itself once its wait is over. Asking resumes it
// no sooner; past the wait it ends with the cancellation, by its own code.
TEST(CancellationTest, AnAwaiterOfTheApplicationIsNotReachedTheCoroutineAsksItself) {
    std::coroutine_handle<> slot;
    cancellation_source stop;
    task<int> waits = asks_its_token_itself(slot, stop.token());

    ASSERT_TRUE(slot);
    stop.cancel();
    EXPECT_FALSE(waits.done());

    slot.resume();
    EXPECT_TRUE(ends_cancelled(waits));

    // Without a request it is the value; a token that is nobody's is never cancelled.
    task<int> unowned = asks_its_token_itself(slot, cancellation_token{});
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
    task<std::size_t> chain = top(p, stop.token());

    sta_loop::run_until([&] { return chain.done(); });

    EXPECT_EQ(chain.result(), 16u);
    EXPECT_EQ(p.told, 0);
    EXPECT_EQ(p.wrote, 1);
}

// Under a token cancelled already the operation is not even made: the co_await ends at
// once, without a trip to the worker.
TEST(CancellationTest, UnderACancelledTokenTheFormWithATokenStartsNothing) {
    probe p;
    cancellation_source stop;
    stop.cancel();

    task<std::size_t> chain = top(p, stop.token());

    EXPECT_TRUE(chain.done());
    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(p.made, 0);
}

// A read the loop has already taken back has nothing out to cut short: asked, it is not
// told, and the co_await that comes after the request still ends with what the read
// brought -- the read was over before the request, and is not undone by it.
TEST(CancellationTest, AnOperationBackBeforeItsCoAwaitKeepsItsAnswerAfterTheRequest) {
    probe p;
    p.gate.set();

    std::byte buf[16]{};
    cancellation_source stop;
    task<std::size_t> read = start_read(p, buf, stop.token());
    sta_loop::run_until([&] { return read.done(); });

    stop.cancel();

    EXPECT_EQ(p.told, 0);
    EXPECT_EQ(p.wrote, 1);

    task<std::size_t> joined = joins(read);

    EXPECT_TRUE(joined.done());
    EXPECT_EQ(joined.result(), 16u);
}

// One that has done its work and is on its way back when the request comes is told -- it is
// still out -- and has nothing left to cut short: it answers what it brought.
TEST(CancellationTest, AnOperationDoneOnTheWorkerKeepsItsAnswerWhenToldOnItsWayBack) {
    probe p;
    p.gate.set();

    std::byte buf[16]{};
    cancellation_source stop;
    task<std::size_t> read = start_read(p, buf, stop.token());

    // Written, so the body is over; the loop has not taken the read back.
    while (p.wrote == 0) std::this_thread::yield();
    EXPECT_FALSE(read.done());

    stop.cancel();
    EXPECT_EQ(p.told, 1);

    sta_loop::run_until([&] { return read.done(); });

    EXPECT_EQ(read.result(), 16u);
    EXPECT_EQ(p.wrote, 1);
}

// What the form with a token returns is a task like any other: kept and awaited later, it
// holds its token, so it does not matter what has become of the source by then.
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

    // Asked while it is kept, it is told at once -- it stands under the token from the
    // moment it is made, awaited or not -- and the co_await that comes later waits for it
    // to come back and ends with the cancellation.
    probe p;
    cancellation_source stop;
    task<std::size_t> chain = keeps_the_read(p, stop.token(), slot);

    p.started.wait();
    stop.cancel();
    EXPECT_EQ(p.told, 1);

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

// A read under the token whose frame is destroyed while it is kept: given up, it leaves the
// token's event as it goes, before the token it holds -- which may be the state's last
// holder -- lets go of the state.
TEST(CancellationTest, AFrameDestroyedOnAKeptReadLeavesTheToken) {
    {
        probe p;
        std::coroutine_handle<> slot;
        cancellation_source stop;
        {
            task<std::size_t> chain = keeps_the_read(p, stop.token(), slot);
            p.started.wait();
        }

        EXPECT_EQ(p.told, 1) << "the read was given up";
        sta_loop::run_until([&] { return p.alive == 0; });

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

        // The kept read now stands in the event of a state only its own token holds.
        stop.reset();
    }

    EXPECT_EQ(p.told, 1);
    sta_loop::run_until([&] { return p.alive == 0; });
}

// Many under one token, leaving in any order: the one in the middle of the event comes back
// and goes first, and the request reaches the two still standing under the token -- the
// first, back and kept, keeps its answer, and only the last, still out, is told.
TEST(CancellationTest, ManyUnderOneTokenLeaveInAnyOrderAndTheRestAreTold) {
    probe first, middle, last;
    middle.gate.set();

    std::byte a[16]{}, b[16]{}, c[16]{};
    cancellation_source stop;

    task<std::size_t> one = start_read(first, a, stop.token());
    task<std::size_t> two = start_read(middle, b, stop.token());
    task<std::size_t> three = start_read(last, c, stop.token());

    // The worker takes them in turn: let the first through only once the middle one would
    // be next, so that it is the middle one that comes back and goes.
    first.gate.set();
    sta_loop::run_until([&] { return two.done(); });
    EXPECT_EQ(two.result(), 16u);
    two = task<std::size_t>(cancellation_detail::canceled<std::size_t>());
    EXPECT_EQ(middle.alive, 0);

    stop.cancel();

    EXPECT_EQ(first.told, 0);
    EXPECT_EQ(middle.told, 0);
    EXPECT_EQ(last.told, 1);

    sta_loop::run_until([&] { return one.done() && three.done(); });

    // The first read came back before the request and keeps what it brought; the last was
    // cut short.
    EXPECT_EQ(one.result(), 16u);
    EXPECT_TRUE(ends_cancelled(three));
    EXPECT_EQ(last.wrote, 0);
}

// ---- The loop's calls under a token, for the operations of this module -----------------

/// A body on the worker, kept behind a read that holds the worker, counting its runs.
task<int> counts_on_the_worker(int& runs, cancellation_token stop) {
    co_return co_await cancellation_detail::call_under([&runs] { return ++runs; },
                                                       std::move(stop));
}

// The lambda forms take the token as the class form does: one not reached by the worker
// never runs, and the co_await ends with the cancellation once it is back.
TEST(CancellationTest, ALambdaOnTheWorkerToldBeforeItIsReachedNeverRuns) {
    probe ahead;
    std::byte ahead_buf[16]{};
    auto holds_the_worker = start_read(ahead, ahead_buf);
    ahead.started.wait();

    int runs = 0;
    cancellation_source stop;
    task<int> counted = counts_on_the_worker(runs, stop.token());

    stop.cancel();
    ahead.gate.set();
    sta_loop::run_until([&] { return counted.done() && holds_the_worker.done(); });

    EXPECT_TRUE(ends_cancelled(counted));
    EXPECT_EQ(runs, 0);

    // And without a request the same form answers what the body made.
    cancellation_source fresh;
    task<int> again = counts_on_the_worker(runs, fresh.token());
    sta_loop::run_until([&] { return again.done(); });
    EXPECT_EQ(again.result(), 1);

    // Under a token cancelled already nothing is made: the co_await ends at once.
    task<int> refused = counts_on_the_worker(runs, stop.token());
    EXPECT_TRUE(refused.done());
    EXPECT_TRUE(ends_cancelled(refused));
    EXPECT_EQ(runs, 1);
}

// An orphanable body under a live token runs and answers; told while it waits for the
// worker, it never runs, and the co_await ends with the cancellation once it is back; under a
// token cancelled already it is not made.
TEST(CancellationTest, TheOrphanableFormTakesTheTokenToo) {
    cancellation_source stop;

    task<int> ran = cancellation_detail::call_under(orphanable, [] { return 5; }, stop.token());
    sta_loop::run_until([&] { return ran.done(); });
    EXPECT_EQ(ran.result(), 5);

    probe ahead;
    std::byte ahead_buf[16]{};
    auto holds_the_worker = start_read(ahead, ahead_buf);
    ahead.started.wait();

    std::atomic<int> runs{0};
    task<int> told = cancellation_detail::call_under(
        orphanable, [&runs] { return ++runs; }, stop.token());

    stop.cancel();
    ahead.gate.set();
    sta_loop::run_until([&] { return told.done() && holds_the_worker.done(); });

    EXPECT_TRUE(ends_cancelled(told));
    EXPECT_EQ(runs, 0);

    bool made = false;
    task<int> not_made = cancellation_detail::call_under(
        orphanable, [&made] { return made = true, 6; }, stop.token());
    EXPECT_TRUE(not_made.done());
    EXPECT_TRUE(ends_cancelled(not_made));
    EXPECT_FALSE(made);
}

namespace {

/// An operation no loop carries: the test does the worker's part by hand, and takes it back
/// by hand. Its body answers the value it was made with.
class carried_by_hand : public async_op_t<int>
{
public:
    explicit carried_by_hand(int value) : value_(value) {}

    int runs = 0;

protected:
    bool execute() override {
        ++runs;
        set_value(int(value_));
        return true;
    }

private:
    int value_;
};

}  // namespace

// A loop that is being stopped settles what comes back and resumes nobody, and leaves the
// answer as the operation made it: one told after its body ran answers its value, one told
// before the worker reached it never runs and answers the cancellation the worker wrote for
// it, and one never told its value.
TEST(CancellationTest, AnOperationToldWhileOutIsSettledWithWhatHappenedToIt) {
    using op = cancellation_detail::operation_under<carried_by_hand>;

    cancellation_source stop;

    auto* const done_first = new op(stop.token(), 3);
    task<int> done_first_task(std::unique_ptr<async_op_t<int>>{done_first});

    auto* const not_reached = new op(stop.token(), 5);
    task<int> not_reached_task(std::unique_ptr<async_op_t<int>>{not_reached});

    EXPECT_TRUE(done_first->packaged_execute());

    stop.cancel();

    EXPECT_TRUE(not_reached->packaged_execute());
    EXPECT_EQ(not_reached->runs, 0) << "an operation told before the worker reached it ran";

    done_first->settle();
    not_reached->settle();

    EXPECT_TRUE(done_first_task.done());
    EXPECT_EQ(done_first_task.result(), 3);

    EXPECT_TRUE(not_reached_task.done());
    EXPECT_TRUE(ends_cancelled(not_reached_task));

    cancellation_source fresh;
    auto* const plain = new op(fresh.token(), 4);
    task<int> plain_task(std::unique_ptr<async_op_t<int>>{plain});

    EXPECT_TRUE(plain->packaged_execute());
    plain->settle();

    EXPECT_TRUE(plain_task.done());
    EXPECT_EQ(plain_task.result(), 4);
}

/// Counts its living copies: what an operation's body captured lives as long as the
/// operation does.
struct counted_capture {
    int* alive;

    explicit counted_capture(int* count) noexcept : alive(count) { ++*alive; }
    counted_capture(const counted_capture& other) noexcept : alive(other.alive) { ++*alive; }
    counted_capture& operator=(const counted_capture&) = delete;
    ~counted_capture() { --*alive; }
};

// An orphan under the token that its task let go of finishes alone; a request that comes
// meanwhile does not take it back: it is deleted when it comes back, as any orphan given up.
TEST(CancellationTest, AnOrphanGivenUpAndThenAskedStillGoesWhenItComesBack) {
    hevent started{true}, gate{true};
    std::atomic<bool> finished{false};
    int alive = 0;
    cancellation_source stop;

    {
        task<int> dropped = cancellation_detail::call_under(
            orphanable,
            [&started, &gate, &finished, keep = counted_capture(&alive)] {
                started.set();
                gate.wait();
                finished = true;
                return 1;
            },
            stop.token());

        started.wait();
    }

    stop.cancel();
    EXPECT_EQ(alive, 1) << "an orphan given up is deleted only when it comes back";

    gate.set();

    // Bounded rather than run_until: an orphan that is never deleted would leave nothing
    // more in the return channel to wait for.
    for (int i = 0; i < 2000 && alive != 0; ++i) {
        sta_loop::run_pending();
        if (alive != 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_TRUE(finished);
    EXPECT_EQ(alive, 0);
}

// ---- What happened, not when the request came ------------------------------------------

namespace {

/// What a task's co_await ended with, in a word: the value, the cancellation, or the code
/// of a failed call to the system or the text of any other failure.
template <class Task>
std::string answer_of(Task& t) {
    try {
        return std::format("value {}", t.result());
    } catch (const operation_canceled_exception&) {
        return "cancelled";
    } catch (const system_exception& failure) {
        return std::format("system error {}", failure.err_code());
    } catch (const std::exception& failure) {
        return failure.what();
    }
}

/// Counts the living copies of a result, on whichever thread one goes.
class counted_result
{
public:
    explicit counted_result(std::atomic<int>& alive) noexcept : alive_(&alive) { ++*alive_; }
    counted_result(counted_result&& other) noexcept : alive_(other.alive_) { ++*alive_; }
    counted_result& operator=(counted_result&&) = delete;
    ~counted_result() { --*alive_; }

private:
    std::atomic<int>* alive_;
};

/// Takes back, without resuming anybody, everything sent before it: an operation given up
/// and left to finish alone is deleted as it comes back, ahead of it.
void take_back_what_was_sent() {
    task<> fence = sta_loop::async_call([] {});
    sta_loop::run_until([&] { return fence.done(); });
}

}  // namespace

// A lambda on the worker cannot be cut short: told while it runs, it finishes, and the
// co_await ends with what it made.
TEST(CancellationTest, ABodyOnTheWorkerToldWhileItRunsAnswersWhatItMade) {
    hevent started{true}, gate{true};
    cancellation_source stop;

    task<int> made = cancellation_detail::call_under(
        [&started, &gate] {
            started.set();
            gate.wait();
            return 7;
        },
        stop.token());

    started.wait();
    stop.cancel();
    gate.set();

    sta_loop::run_until([&] { return made.done(); });

    EXPECT_EQ(answer_of(made), "value 7");
}

// An orphan told while it stands in a call has that call cut short, and answers what the
// call did: cut short, it fails as a call to the system cut short does, and the co_await
// ends with the cancellation; done first, its value is the answer.
TEST(CancellationTest, AnOrphanToldWhileItRunsAnswersWhatHappenedToItsCall) {
    for (const bool cut : {true, false}) {
        SCOPED_TRACE(cut ? "its call cut short" : "its call done first");

        hevent started{true}, gate{true};
        bool saw_it_told = false;
        cancellation_source stop;

        task<int> reading = cancellation_detail::call_under(
            orphanable,
            [&started, &gate, &saw_it_told, cut](const orphan_stage& stage) -> int {
                started.set();

                // Stands for the call to the system the telling cuts short.
                gate.wait();

                saw_it_told = stage.cut_short();

                if (cut) throw system_exception("ReadFile", ERROR_OPERATION_ABORTED);

                return 1;
            },
            stop.token());

        started.wait();
        stop.cancel();
        gate.set();

        sta_loop::run_until([&] { return reading.done(); });

        EXPECT_TRUE(saw_it_told);
        EXPECT_EQ(answer_of(reading), cut ? "cancelled" : "value 1");
    }
}

// A failure of the body's own stays its answer, request or no request: the lambda on the
// worker and the orphan both end with what they threw.
TEST(CancellationTest, AFailureOfTheBodysOwnStaysTheAnswerAfterTheRequest) {
    {
        hevent started{true}, gate{true};
        cancellation_source stop;

        task<int> failing = cancellation_detail::call_under(
            [&started, &gate]() -> int {
                started.set();
                gate.wait();
                throw std::runtime_error("its own");
            },
            stop.token());

        started.wait();
        stop.cancel();
        gate.set();

        sta_loop::run_until([&] { return failing.done(); });

        EXPECT_EQ(answer_of(failing), "its own");
    }

    hevent started{true}, gate{true};
    cancellation_source stop;

    task<int> refused = cancellation_detail::call_under(
        orphanable,
        [&started, &gate]() -> int {
            started.set();
            gate.wait();
            throw system_exception("CreateFileW", ERROR_ACCESS_DENIED);
        },
        stop.token());

    started.wait();
    stop.cancel();
    gate.set();

    sta_loop::run_until([&] { return refused.done(); });

    EXPECT_EQ(answer_of(refused), std::format("system error {}", int{ERROR_ACCESS_DENIED}));
}

// An orphan told keeps what it makes for its co_await; given up afterwards, it lets go of it
// as any orphan given up does -- on the thread that makes it, as soon as it is made, and
// not when the loop meets the operation in the return channel.
TEST(CancellationTest, AnOrphanToldAndThenGivenUpLetsGoOfWhatItMakesAtOnce) {
    hevent started{true}, gate{true};
    std::atomic<int> alive{0};
    std::atomic<bool> made{false};
    cancellation_source stop;

    {
        auto making = cancellation_detail::call_under(
            orphanable,
            [&started, &gate, &alive, &made] {
                started.set();
                gate.wait();

                counted_result result(alive);
                made = true;
                return result;
            },
            stop.token());

        started.wait();
        stop.cancel();
    }

    gate.set();

    // Bounded rather than waited for: a result that stays would stay until the loop runs,
    // and the loop is not run here.
    for (int i = 0; i < 2000 && !(made && alive == 0); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    EXPECT_TRUE(made);
    EXPECT_EQ(alive, 0) << "what an orphan given up made stays until it comes back";

    take_back_what_was_sent();
    EXPECT_EQ(alive, 0);
}

// A token that has no source is never cancelled: the form under it is the form without
// one, and stands nowhere.
TEST(CancellationTest, UnderATokenOfNobodyTheFormIsThePlainOne) {
    probe p;
    p.gate.set();

    std::byte buf[16]{};
    task<std::size_t> read = start_read(p, buf, cancellation_token{});
    task<int> lambda = cancellation_detail::call_under([] { return 9; }, cancellation_token{});

    sta_loop::run_until([&] { return read.done() && lambda.done(); });

    EXPECT_EQ(read.result(), 16u);
    EXPECT_EQ(lambda.result(), 9);
}

// ---- A source under a parent token -----------------------------------------------------

namespace {

/// Whether anybody stands in the event of a token's state: an operation, a wait, a source
/// made under the token.
bool anybody_stands(const cancellation_token& token) {
    return static_cast<bool>(cancellation_detail::state_of(token)->told());
}

/// Counts the states it was put into that are still alive: a callback of its own in the
/// state's event, which the event lets go of -- and with it what the callback captured --
/// when the state goes. Only for a state not cancelled meanwhile: a fire lets go of what it
/// has called.
void count_while_alive(const cancellation_token& token, int& alive) {
    cancellation_detail::state_of(token)->told().add(
        [keep = counted_capture(&alive)]() noexcept {});
}

/// The thread's queue, as a wait of wxl.ui sees it: a told wait hands its coroutine here
/// rather than resuming it, and the test takes the turns.
using turns = std::vector<std::coroutine_handle<>>;

void take_turns(turns& queue) {
    while (!queue.empty()) {
        const std::coroutine_handle<> next = queue.back();
        queue.pop_back();
        next.resume();
    }
}

/// The awaiter of such a wait: it ends when the test resumes it from `slot` -- its event
/// came -- or, told, on the queue's turn.
struct queued_awaiter {
    turns* queue;
    std::coroutine_handle<>* slot;
    bool told = false;

    bool await_ready() const noexcept { return told; }
    void await_suspend(std::coroutine_handle<> here) noexcept { *slot = here; }
    void await_resume() const noexcept {}

    void cancel() noexcept {
        told = true;
        if (*slot) queue->push_back(std::exchange(*slot, {}));
    }
};

/// A wait of wxl under a token over that awaiter: what the event waits of wxl.ui are made of.
class queued_wait : public cancellation_detail::wait_under<queued_wait>
{
    friend cancellation_detail::wait_under<queued_wait>;

public:
    queued_wait(turns& queue, std::coroutine_handle<>& slot, cancellation_token stop) noexcept
        : stop_(std::move(stop)), awaiter_{.queue = &queue, .slot = &slot} {}

    queued_wait(const queued_wait&) = delete;
    queued_wait& operator=(const queued_wait&) = delete;

    ~queued_wait() { this->leave(); }

private:
    queued_awaiter& awaiter() noexcept { return awaiter_; }

    cancellation_detail::cancellation_state* state() const noexcept {
        return cancellation_detail::state_of(stop_);
    }

    cancellation_token stop_;
    queued_awaiter awaiter_;
};

task<> waits_its_turn(turns& queue, std::coroutine_handle<>& slot, cancellation_token stop,
                      std::string& ended) {
    try {
        co_await queued_wait{queue, slot, std::move(stop)};
        ended = "came";
    } catch (const operation_canceled_exception&) {
        ended = "cancelled";
    }
}

}  // namespace

// Cancelling the parent cancels the child, and what stands under the child is cut short as
// if the child had been cancelled itself: told inside the parent's cancel(), resumed by
// nobody there, ended through the loop.
TEST(CancellationTest, CancellingTheParentCancelsTheChild) {
    probe p;
    cancellation_source parent;
    cancellation_source child(parent.token());
    task<std::size_t> chain = top(p, child.token());

    p.started.wait();
    EXPECT_FALSE(child.is_canceled());

    parent.cancel();

    EXPECT_TRUE(child.is_canceled());
    EXPECT_EQ(p.told, 1);
    EXPECT_FALSE(chain.done()) << "cancel() resumed the chain itself";

    sta_loop::run_until([&] { return chain.done(); });

    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(p.wrote, 0);
    EXPECT_EQ(p.alive, 0);
}

// Cancelling the child reaches its own and not the parent's. The child goes on standing in
// the parent's event until it goes, and the parent's cancel() then cancels it a second time,
// which tells nobody.
TEST(CancellationTest, CancellingTheChildLeavesTheParentAlone) {
    probe under_parent, under_child;
    cancellation_source parent;
    cancellation_source child(parent.token());

    {
        std::byte a[16]{}, b[16]{};
        task<std::size_t> parents = start_read(under_parent, a, parent.token());
        under_parent.started.wait();
        task<std::size_t> childs = start_read(under_child, b, child.token());

        child.cancel();

        EXPECT_TRUE(child.is_canceled());
        EXPECT_FALSE(parent.is_canceled());
        EXPECT_EQ(under_child.told, 1);
        EXPECT_EQ(under_parent.told, 0);

        under_parent.gate.set();
        sta_loop::run_until([&] { return parents.done() && childs.done(); });

        EXPECT_EQ(parents.result(), 16u);
        EXPECT_TRUE(ends_cancelled(childs));
        EXPECT_EQ(under_child.ran, 0);
    }

    EXPECT_TRUE(anybody_stands(parent.token())) << "the child left the parent's event early";

    parent.cancel();

    EXPECT_TRUE(child.is_canceled());
    EXPECT_EQ(under_child.told, 1);
    EXPECT_FALSE(anybody_stands(child.token()));

    probe late;
    task<std::size_t> refused = top(late, child.token());
    EXPECT_TRUE(ends_cancelled(refused));
    EXPECT_EQ(late.made, 0);
}

// Under a parent cancelled already the child is born cancelled and stands nowhere; under a
// token of nobody it is a source like any other.
TEST(CancellationTest, UnderACancelledParentTheChildIsBornCancelled) {
    cancellation_source parent;
    parent.cancel();

    cancellation_source child(parent.token());

    EXPECT_TRUE(child.is_canceled());
    EXPECT_FALSE(anybody_stands(parent.token())) << "the child stands in an event that has fired";

    probe p;
    task<std::size_t> chain = top(p, child.token());
    EXPECT_TRUE(chain.done());
    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(p.made, 0);

    cancellation_source unowned(cancellation_token{});
    EXPECT_FALSE(unowned.is_canceled());
    unowned.cancel();
    EXPECT_TRUE(unowned.is_canceled());
}

// A child that goes before the parent is cancelled takes its callback out of the parent's
// event -- cancelled or not, and once its last token has gone, the source or not -- and the
// parent's cancel() reaches nothing that is gone.
TEST(CancellationTest, AChildGoneBeforeTheParentIsCancelledLeavesItsEvent) {
    cancellation_source parent;

    {
        cancellation_source child(parent.token());
        EXPECT_TRUE(anybody_stands(parent.token()));
    }
    EXPECT_FALSE(anybody_stands(parent.token()));

    {
        cancellation_source child(parent.token());
        child.cancel();
    }
    EXPECT_FALSE(anybody_stands(parent.token()));

    std::optional<cancellation_token> kept;
    {
        cancellation_source child(parent.token());
        kept = child.token();
    }
    EXPECT_TRUE(anybody_stands(parent.token())) << "the child left while its token was held";
    kept.reset();
    EXPECT_FALSE(anybody_stands(parent.token()));

    parent.cancel();
    EXPECT_TRUE(parent.is_canceled());
}

// Three generations: cancelling the middle one cancels the youngest and not the eldest, and
// cancelling the eldest reaches the youngest through the middle one -- told inside a fire
// inside a fire, resuming nobody.
TEST(CancellationTest, ThreeGenerations) {
    {
        cancellation_source eldest;
        cancellation_source middle(eldest.token());
        cancellation_source youngest(middle.token());

        middle.cancel();

        EXPECT_FALSE(eldest.is_canceled());
        EXPECT_TRUE(youngest.is_canceled());
    }

    // A read under no token keeps the worker, so the three wait in its queue behind it.
    probe ahead, e, m, y;
    std::byte ahead_buf[16]{};
    task<std::size_t> holds_the_worker = start_read(ahead, ahead_buf);
    ahead.started.wait();

    cancellation_source eldest;
    cancellation_source middle(eldest.token());
    cancellation_source youngest(middle.token());

    task<std::size_t> under_eldest = top(e, eldest.token());
    task<std::size_t> under_middle = top(m, middle.token());
    task<std::size_t> under_youngest = top(y, youngest.token());

    eldest.cancel();

    EXPECT_TRUE(middle.is_canceled());
    EXPECT_TRUE(youngest.is_canceled());
    EXPECT_EQ(e.told + m.told + y.told, 3);
    EXPECT_FALSE(under_eldest.done() || under_middle.done() || under_youngest.done())
        << "cancel() resumed a chain itself";

    ahead.gate.set();
    sta_loop::run_until([&] {
        return holds_the_worker.done() && under_eldest.done() && under_middle.done() &&
               under_youngest.done();
    });

    EXPECT_TRUE(ends_cancelled(under_eldest));
    EXPECT_TRUE(ends_cancelled(under_middle));
    EXPECT_TRUE(ends_cancelled(under_youngest));
    EXPECT_EQ(e.ran + m.ran + y.ran, 0);
}

// The parent's source and every token of it gone but the child's hold: the child goes on,
// the parent's state lives as long as the child's, and both go when the child does -- the
// parent holds nothing of the child, so there is no cycle to leak.
TEST(CancellationTest, AChildHoldsItsParentAndNothingLeaks) {
    int parents = 0, children = 0;

    {
        std::optional<cancellation_source> parent(std::in_place);
        cancellation_source child(parent->token());
        count_while_alive(parent->token(), parents);
        count_while_alive(child.token(), children);

        parent.reset();
        EXPECT_EQ(parents, 1) << "the parent's state went while the child stood in its event";

        probe p;
        p.gate.set();
        task<std::size_t> chain = top(p, child.token());
        sta_loop::run_until([&] { return chain.done(); });
        EXPECT_EQ(chain.result(), 16u);
    }

    EXPECT_EQ(children, 0);
    EXPECT_EQ(parents, 0);

    // And cancelled by itself, with nothing of the parent left but its hold.
    {
        std::optional<cancellation_source> parent(std::in_place);
        cancellation_source child(parent->token());
        count_while_alive(parent->token(), parents);
        parent.reset();

        probe p;
        task<std::size_t> chain = top(p, child.token());
        p.started.wait();
        child.cancel();
        EXPECT_EQ(p.told, 1);

        sta_loop::run_until([&] { return chain.done(); });
        EXPECT_TRUE(ends_cancelled(chain));
        EXPECT_EQ(parents, 1);
    }

    EXPECT_EQ(parents, 0);
}

// Many children under one parent, going front to back and back to front: each takes its own
// callback out of the parent's event and no other, and the parent's cancel() reaches those
// still there -- one cancelled by itself already among them.
TEST(CancellationTest, ManyChildrenUnderOneParentGoInEitherOrder) {
    for (const bool backwards : {false, true}) {
        SCOPED_TRACE(backwards ? "back to front" : "front to back");

        cancellation_source parent;
        std::vector<std::optional<cancellation_source>> children(8);

        const std::size_t n = children.size();
        const auto nth = [&](std::size_t k) -> auto& {
            return children[backwards ? n - 1 - k : k];
        };

        for (auto& child : children) child.emplace(parent.token());
        for (std::size_t k = 0; k < n; ++k) {
            EXPECT_TRUE(anybody_stands(parent.token()));
            nth(k).reset();
        }
        EXPECT_FALSE(anybody_stands(parent.token()));

        for (auto& child : children) child.emplace(parent.token());
        for (std::size_t k = 0; k < n; k += 2) nth(k).reset();
        nth(1)->cancel();

        parent.cancel();

        for (const auto& child : children) {
            if (child) {
                EXPECT_TRUE(child->is_canceled());
            }
        }

        for (std::size_t k = 0; k < n; ++k) nth(k).reset();
    }
}

// A wait of wxl under the child's token: without a request it ends when its event comes; told
// when the parent is cancelled, it is handed to the queue inside the parent's cancel() and
// ends with the cancellation on the queue's turn; begun under the child cancelled, it ends at
// once without suspending.
TEST(CancellationTest, AWaitUnderTheChildIsToldWithTheParent) {
    turns queue;
    std::coroutine_handle<> slot;
    cancellation_source parent;
    cancellation_source child(parent.token());

    std::string came;
    task<> first = waits_its_turn(queue, slot, child.token(), came);
    ASSERT_TRUE(slot);
    std::exchange(slot, {}).resume();
    EXPECT_EQ(came, "came");

    std::string told;
    task<> second = waits_its_turn(queue, slot, child.token(), told);
    EXPECT_TRUE(anybody_stands(child.token()));

    parent.cancel();

    EXPECT_FALSE(second.done()) << "cancel() resumed the wait itself";
    ASSERT_EQ(queue.size(), 1u);
    take_turns(queue);
    EXPECT_EQ(told, "cancelled");

    std::string at_once;
    task<> third = waits_its_turn(queue, slot, child.token(), at_once);
    EXPECT_TRUE(third.done());
    EXPECT_EQ(at_once, "cancelled");
    EXPECT_TRUE(queue.empty());
}

// What the child stands on: told inside the parent's fire, it tells its own -- an operation,
// a wait, a child of its own -- and destroys nobody, while its callback lies in the event the
// parent fires. Its sources then go before what stands under them, and each state, going
// last, finds its parent's event emptied by the fire and takes nothing out of it.
TEST(CancellationTest, TheParentsFireTellsTheChildsOwnAndDestroysNobody) {
    // A read under no token keeps the worker, so the reads under the tokens wait behind it.
    probe ahead, before, under_child, under_grandchild, after;
    std::byte ahead_buf[16]{};
    task<std::size_t> holds_the_worker = start_read(ahead, ahead_buf);
    ahead.started.wait();

    turns queue;
    std::coroutine_handle<> slot;
    std::string ended;

    cancellation_source parent;
    std::byte buf[16]{};
    task<std::size_t> first = start_read(before, buf, parent.token());

    std::optional<cancellation_source> child(std::in_place, parent.token());
    std::optional<cancellation_source> grandchild(std::in_place, child->token());

    task<std::size_t> childs = top(under_child, child->token());
    task<> waits = waits_its_turn(queue, slot, child->token(), ended);
    task<std::size_t> grandchilds = top(under_grandchild, grandchild->token());
    task<std::size_t> last = top(after, parent.token());

    parent.cancel();

    EXPECT_EQ(before.told + under_child.told + under_grandchild.told + after.told, 4);
    EXPECT_EQ(queue.size(), 1u);
    EXPECT_FALSE(first.done() || childs.done() || waits.done() || grandchilds.done() ||
                 last.done())
        << "cancel() resumed somebody";
    EXPECT_FALSE(anybody_stands(parent.token()));

    child.reset();
    grandchild.reset();

    take_turns(queue);
    ahead.gate.set();
    sta_loop::run_until([&] {
        return holds_the_worker.done() && first.done() && childs.done() && grandchilds.done() &&
               last.done();
    });

    EXPECT_TRUE(ends_cancelled(first));
    EXPECT_TRUE(ends_cancelled(childs));
    EXPECT_TRUE(ends_cancelled(grandchilds));
    EXPECT_TRUE(ends_cancelled(last));
    EXPECT_EQ(ended, "cancelled");
    EXPECT_EQ(before.ran + under_child.ran + under_grandchild.ran + after.ran, 0);
}

// The token is one thread's: a build that checks coroutines stops a cancel() from any other,
// and a source made under a token there, which would join the token's event -- assert in a
// Debug build, core::abort with the caller's line under STRICT_CORO.

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

constexpr std::uint_least32_t line_of_the_foreign_child = std::source_location::current().line() + 1;
void make_a_child_there(cancellation_source& parent) { cancellation_source child(parent.token()); }

void make_a_child_on_another_thread() {
    report_failures_to_stderr();

    cancellation_source parent;
    std::thread([&parent] { make_a_child_there(parent); }).join();
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

TEST_F(CancellationDeathTest, AChildIsMadeOnItsCoroutinesThread) {
    EXPECT_DEATH(make_a_child_on_another_thread(),
                 "made under a token on a thread other than its coroutines'");
}

TEST_F(CancellationStrictDeathTest, TheReportNamesTheLineOfTheForeignChild) {
    EXPECT_DEATH(make_a_child_on_another_thread(),
                 std::format("cancellation_tests\\.cpp\\({}\\): cancellation_source: made under a token",
                             line_of_the_foreign_child));
}

static_assert(sizeof(cancellation_token) == sizeof(void*));

// A token is not a source: a child is made by name, from its parent's token.
static_assert(std::is_constructible_v<cancellation_source, cancellation_token>);
static_assert(!std::is_convertible_v<cancellation_token, cancellation_source>);

// The form with a token is the same task: the token and the place in its event live in the
// operation, and the plain form has neither.
static_assert(sizeof(task<std::size_t>) == sizeof(void*));
static_assert(std::is_same_v<decltype(start_read(std::declval<probe&>(), {}, cancellation_token{})),
                             task<std::size_t>>);
static_assert(std::is_same_v<decltype(cancellation_detail::call_under(std::declval<int (*)()>(),
                                                                    cancellation_token{})),
                             task<int>>);
static_assert(sizeof(cancellation_detail::operation_under<gated_read>) ==
              sizeof(gated_read) + sizeof(cancellation_token) + 3 * sizeof(void*));

// The loop's calls take no token: a body of the application's is its own code's to stop.
namespace {

template <class Fn>
concept a_loop_call_takes_a_token =
    requires(Fn fn, cancellation_token token) { sta_loop::async_call(fn, token); } ||
    requires(Fn fn, cancellation_token token) { sta_loop::async_call(orphanable, fn, token); } ||
    requires(Fn fn, cancellation_token token) { sta_loop::call_here(fn, token); };

}  // namespace

static_assert(!a_loop_call_takes_a_token<int (*)()>);

