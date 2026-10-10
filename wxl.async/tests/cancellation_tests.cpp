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

task<std::size_t> start_read(probe& p, std::span<std::byte> into) {
    return sta_loop::async_run(std::unique_ptr<async_op_t<std::size_t>>(new gated_read(p, into)));
}

/// The same read under a token, in the form the operations of this module take one --
/// `async_file::read_all(path, stop)` -- and the same task.
task<std::size_t> start_read(probe& p, std::span<std::byte> into, cancellation_token stop) {
    return sta_loop::async_run<gated_read>(std::move(stop), p, into);
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
// told, and from then on it answers the cancellation -- by level: a co_await that ends
// after the request ends with it, whatever the read brought.
TEST(CancellationTest, AnOperationAlreadyBackIsNotToldAndAnswersTheCancellation) {
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
    EXPECT_TRUE(ends_cancelled(joined));
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
// and goes first, and the request tells the two still out -- and only them.
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
    two = task<std::size_t>(sta_loop::canceled<std::size_t>());
    EXPECT_EQ(middle.alive, 0);

    stop.cancel();

    EXPECT_EQ(middle.told, 0);
    EXPECT_EQ(last.told, 1);

    sta_loop::run_until([&] { return one.done() && three.done(); });

    // The first read came back before the request and is answered as the cancellation: by
    // level, whatever it brought.
    EXPECT_TRUE(ends_cancelled(one));
    EXPECT_TRUE(ends_cancelled(three));
    EXPECT_EQ(last.wrote, 0);
}

// ---- The forms of the loop's own calls -----------------------------------------------

/// A body on the worker, kept behind a read that holds the worker, counting its runs.
task<int> counts_on_the_worker(int& runs, cancellation_token stop) {
    co_return co_await sta_loop::async_call([&runs] { return ++runs; }, std::move(stop));
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

// An orphanable body under a token cancelled already is not made; under a live one it runs
// and answers.
TEST(CancellationTest, TheOrphanableFormTakesTheTokenToo) {
    cancellation_source stop;

    task<int> ran = sta_loop::async_call(orphanable, [] { return 5; }, stop.token());
    sta_loop::run_until([&] { return ran.done(); });
    EXPECT_EQ(ran.result(), 5);

    stop.cancel();

    bool made = false;
    task<int> not_made = sta_loop::async_call(orphanable, [&made] { return made = true, 6; }, stop.token());
    EXPECT_TRUE(not_made.done());
    EXPECT_TRUE(ends_cancelled(not_made));
    EXPECT_FALSE(made);
}

// The form of call_here(): the body runs inside the call unless the token is cancelled
// already, and the answer stands under the token while the task keeps it -- a co_await
// that ends after the request ends with the cancellation. A body that cancels its own
// token as it runs answers the cancellation as well.
TEST(CancellationTest, AnAnswerMadeHereStandsUnderTheTokenWhileItIsKept) {
    cancellation_source stop;
    int runs = 0;

    task<int> kept = sta_loop::call_here([&runs] { return ++runs; }, stop.token());
    EXPECT_TRUE(kept.done());
    EXPECT_EQ(runs, 1);

    stop.cancel();

    task<int> joined = [](task<int>& t) -> task<int> { co_return co_await t; }(kept);
    EXPECT_TRUE(ends_cancelled(joined));

    task<int> refused = sta_loop::call_here([&runs] { return ++runs; }, stop.token());
    EXPECT_TRUE(ends_cancelled(refused));
    EXPECT_EQ(runs, 1);

    cancellation_source own;
    task<int> self_cancelled = sta_loop::call_here(
        [&own] {
            own.cancel();
            return 3;
        },
        own.token());
    EXPECT_TRUE(ends_cancelled(self_cancelled));
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
        task<int> dropped = sta_loop::async_call(
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

// A token that has no source is never cancelled: the form under it is the form without
// one, and stands nowhere.
TEST(CancellationTest, UnderATokenOfNobodyTheFormIsThePlainOne) {
    probe p;
    p.gate.set();

    std::byte buf[16]{};
    task<std::size_t> read = start_read(p, buf, cancellation_token{});
    task<int> lambda = sta_loop::async_call([] { return 9; }, cancellation_token{});

    sta_loop::run_until([&] { return read.done() && lambda.done(); });

    EXPECT_EQ(read.result(), 16u);
    EXPECT_EQ(lambda.result(), 9);
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

// The form with a token is the same task: the token and the place in its event live in the
// operation, and the plain form has neither.
static_assert(sizeof(task<std::size_t>) == sizeof(void*));
static_assert(std::is_same_v<decltype(start_read(std::declval<probe&>(), {}, cancellation_token{})),
                             task<std::size_t>>);
static_assert(std::is_same_v<decltype(sta_loop::async_call(std::declval<int (*)()>(), cancellation_token{})),
                             task<int>>);
static_assert(sizeof(cancellation_detail::operation_under<gated_read>) ==
              sizeof(gated_read) + sizeof(cancellation_token) + 3 * sizeof(void*));

