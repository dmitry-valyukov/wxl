// Asking a chain of coroutines to end, rather than destroying it: a token passed down the
// chain, an operation of this module made in its form with a token and registered with it,
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
// aborted or never started, and its own answer -- its value or its own failure -- if it
// got there first, request or no request.
#include <crtdbg.h>

#include <gtest/gtest.h>

#include "platform.h"

#include "loop_environment.h"

import std;
import wxl.core;
import wxl.async;

#include "sta_box.h"

using namespace wxl::core;
using namespace wxl::async;

namespace {

// A source or a token that may not be there is one pointer, as each itself is: the empty value is
// the null state (detail::cancellation_sentinel) -- for a source the one it is moved into, for a
// token the one a default token has -- and the sentinel reads the object as that pointer.
static_assert(sizeof(cancellation_source) == sizeof(void*));
static_assert(sizeof(nullable<cancellation_source>) == sizeof(cancellation_source));
static_assert(sizeof(cancellation_token) == sizeof(void*));
static_assert(sizeof(nullable<cancellation_token>) == sizeof(cancellation_token));

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
    std::atomic<int> canceled{0};

    /// Operations of this probe not yet deleted.
    std::atomic<int> alive{0};
};

/// The read: waits at the gate, then writes into the caller's buffer -- in the frame --
/// unless it was asked to stop meanwhile, in which case it comes back aborted, with
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

        if (stop_requested()) {
            set_error(std::make_exception_ptr(operation_canceled_exception()));
            return true;
        }

        std::ranges::fill(into_, std::byte{42});
        ++p_.wrote;

        set_value(into_.size());
        return true;
    }

    void on_cancel() noexcept override {
        ++p_.canceled;
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
/// `async_file::read_all(path, token)` -- and the same task.
task<std::size_t> start_read(probe& p, std::span<std::byte> into, cancellation_token token) {
    return sta_loop::async_run<gated_read>(std::move(token), p, into);
}

/// Three links, the token passed down by hand; the bottom one reads into its frame.
task<std::size_t> bottom(probe& p, cancellation_token token) {
    std::byte buf[16]{};
    co_return co_await start_read(p, buf, token);
}

task<std::size_t> middle(probe& p, cancellation_token token) {
    co_return co_await bottom(p, token);
}

task<std::size_t> top(probe& p, cancellation_token token) {
    co_return co_await middle(p, token);
}

/// Undoes what the chain did: an operation of its own, under no token.
task<> roll_back(probe& undo, std::vector<std::string>& log) {
    std::byte buf[8]{};
    co_await start_read(undo, buf);
    log.push_back("rolled back");
}

/// Catches the cancellation, cleans up with a co_await -- after the handler, since C++
/// allows no co_await inside one -- and passes it on.
task<std::size_t> middle_that_rolls_back(probe& p, probe& undo, cancellation_token token,
                                         std::vector<std::string>& log) {
    std::exception_ptr failure;

    try {
        co_return co_await bottom(p, token);
    } catch (const operation_canceled_exception&) {
        log.push_back("cancelled");
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

/// Work the chain hands to another owner: one not given the token, and one that asked for it.
task<> handed_off(probe& p, std::size_t& got, bool& ended) {
    std::byte buf[16]{};
    got = co_await start_read(p, buf);
    ended = true;
}

task<> cancelable_handed_off(probe& p, cancellation_token token, bool& canceled) {
    std::byte buf[16]{};

    try {
        co_await start_read(p, buf, token);
    } catch (const operation_canceled_exception&) {
        canceled = true;
    }
}

/// The chain's own read goes out first, then those of the work it hands to `others`, which
/// outlive it: the worker takes them in turn.
task<std::size_t> hands_off_two(probe& own, probe& unasked, probe& asking, cancellation_token token,
                                std::vector<task<>>& others, std::size_t& unasked_got,
                                bool& unasked_ended, bool& asking_canceled) {
    task<std::size_t> own_read = bottom(own, token);

    others.push_back(cancelable_handed_off(asking, token, asking_canceled));
    others.push_back(handed_off(unasked, unasked_got, unasked_ended));

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
task<int> asks_its_token_itself(std::coroutine_handle<>& slot, cancellation_token token) {
    const int got = co_await parked{slot};
    token.throw_if_canceled();
    co_return got;
}

/// Starts the read under the token, keeps it, waits for something else first, and only
/// then awaits the read.
task<std::size_t> keeps_the_read(probe& p, cancellation_token token,
                                 std::coroutine_handle<>& slot) {
    std::byte buf[16]{};
    task<std::size_t> read = start_read(p, buf, std::move(token));

    co_await parked{slot};
    co_return co_await read;
}

/// Moves the read into a container before awaiting it there.
task<std::size_t> moves_the_read(probe& p, cancellation_token token) {
    std::byte buf[16]{};
    std::vector<task<std::size_t>> reads;

    reads.push_back(start_read(p, buf, std::move(token)));

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
// loop aborted, the bottom link's wait ends with the cancellation, and the exception
// climbs to the top.
TEST(CancellationTest, TheRequestReachesTheOperationAtTheBottomOfAChain) {
    probe p;
    cancellation_source source;
    task<std::size_t> chain = top(p, source.token());

    p.started.wait();
    EXPECT_FALSE(chain.done());

    source.cancel();

    EXPECT_EQ(p.canceled, 1);
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
    cancellation_source source;
    task<std::size_t> chain = middle_that_rolls_back(p, undo, source.token(), log);

    p.started.wait();
    source.cancel();

    sta_loop::run_until([&] { return chain.done(); });

    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(log, (std::vector<std::string>{"cancelled", "rolled back"}));
    EXPECT_EQ(undo.ran, 1);
    EXPECT_EQ(undo.wrote, 1);
    EXPECT_EQ(undo.canceled, 0) << "the cleanup ran under the token";
    EXPECT_EQ(p.alive + undo.alive, 0);
}

// Two chains cancelled at once: one listens and ends, the other stands on a read it was never
// asked to abort. The sweep finds the first ended, and destroys the second, whose read
// is given up the usual way.
TEST(CancellationTest, SweepingDestroysOnlyWhatHasNotEnded) {
    probe listens, does_not;
    cancellation_source source;

    std::vector<task<std::size_t>> owned;
    owned.push_back(top(listens, source.token()));
    owned.push_back(deaf(does_not, source.token()));

    // One worker: the second read is reached once the first has come back.
    listens.started.wait();
    source.cancel();

    sta_loop::run_until([&] { return owned[0].done(); });

    does_not.started.wait();
    EXPECT_FALSE(owned[1].done());
    EXPECT_EQ(listens.canceled, 1);
    EXPECT_EQ(does_not.canceled, 0);

    // The second step: what ended is read and let go of; what still stands is destroyed.
    std::erase_if(owned, [](task<std::size_t>& t) {
        if (!t.done()) return false;
        EXPECT_TRUE(ends_cancelled(t));
        return true;
    });

    ASSERT_EQ(owned.size(), 1u);
    owned.clear();

    EXPECT_EQ(does_not.canceled, 1) << "giving the read up asks it to stop";
    EXPECT_EQ(does_not.wrote, 0);
    EXPECT_EQ(listens.alive + does_not.alive, 0);
}

// Asked after the read was made and before the worker reached it: the read is cancelled at
// once and never runs. The co_await still lasts until the read has come back -- without
// holding the thread -- and only then ends with the cancellation.
TEST(CancellationTest, ARequestBeforeTheWorkerReachesTheReadKeepsItFromRunning) {
    probe ahead, p;
    std::byte ahead_buf[16]{};

    // Keeps the worker busy, so that the chain's read waits in the queue behind it.
    auto holds_the_worker = start_read(ahead, ahead_buf);
    ahead.started.wait();

    cancellation_source source;
    task<std::size_t> chain = top(p, source.token());

    source.cancel();

    EXPECT_EQ(p.made, 1);
    EXPECT_EQ(p.canceled, 1);
    EXPECT_FALSE(chain.done());

    ahead.gate.set();
    sta_loop::run_until([&] { return chain.done() && holds_the_worker.done(); });

    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(p.ran, 0) << "a read cancelled before the worker reached it was started";
    EXPECT_EQ(p.alive, 0);
}

// A chain that has ended is not reached: its read left the token's event when its task went.
// Asking twice is asking once.
TEST(CancellationTest, CancellingAChainThatHasEndedDoesNothing) {
    probe p;
    p.gate.set();

    cancellation_source source;
    task<std::size_t> chain = top(p, source.token());

    sta_loop::run_until([&] { return chain.done(); });

    source.cancel();
    source.cancel();

    EXPECT_EQ(p.canceled, 0);
    EXPECT_EQ(chain.result(), 16u);
    EXPECT_TRUE(source.is_canceled());
}

// Work the chain started and handed to another owner is not the chain's: without the token it
// reads on and ends with its value, while the work that took the token is cancelled with the
// chain.
TEST(CancellationTest, WorkHandedOffIsNotCancelledUnlessItTookTheToken) {
    probe own, unasked, asking;
    std::size_t unasked_got = 0;
    bool unasked_ended = false, asking_canceled = false;
    std::vector<task<>> others;

    cancellation_source source;
    task<std::size_t> chain = hands_off_two(own, unasked, asking, source.token(), others, unasked_got,
                                            unasked_ended, asking_canceled);

    own.started.wait();
    source.cancel();

    EXPECT_EQ(own.canceled, 1);
    EXPECT_EQ(asking.canceled, 1);
    EXPECT_EQ(unasked.canceled, 0);

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

// A frame destroyed while its read is registered with the token gives the read up, and the read
// leaves the token's event as it goes: asking afterwards reaches nothing that is gone.
TEST(CancellationTest, AFrameDestroyedWhileWaitingLeavesTheToken) {
    probe p;
    cancellation_source source;
    {
        task<std::size_t> chain = top(p, source.token());
        p.started.wait();
    }

    EXPECT_EQ(p.canceled, 1) << "the read was given up";

    source.cancel();

    EXPECT_EQ(p.canceled, 1);
    EXPECT_EQ(p.alive, 0);
}

// What wxl does not make, it does not reach: the application's own awaiter is not under the
// token, and the coroutine asks the token itself once its wait is over. Asking resumes it
// no sooner; past the wait it ends with the cancellation, by its own code.
TEST(CancellationTest, AnAwaiterOfTheApplicationIsNotReachedTheCoroutineAsksItself) {
    std::coroutine_handle<> slot;
    cancellation_source source;
    task<int> waits = asks_its_token_itself(slot, source.token());

    ASSERT_TRUE(slot);
    source.cancel();
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
// operation is never cancelled.
TEST(CancellationTest, TheFormWithATokenAnswersLikeThePlainOne) {
    probe p;
    p.gate.set();

    cancellation_source source;
    task<std::size_t> chain = top(p, source.token());

    sta_loop::run_until([&] { return chain.done(); });

    EXPECT_EQ(chain.result(), 16u);
    EXPECT_EQ(p.canceled, 0);
    EXPECT_EQ(p.wrote, 1);
}

// Under a token cancelled already the operation is not even made: the co_await ends at
// once, without a trip to the worker.
TEST(CancellationTest, UnderACancelledTokenTheFormWithATokenStartsNothing) {
    probe p;
    cancellation_source source;
    source.cancel();

    task<std::size_t> chain = top(p, source.token());

    EXPECT_TRUE(chain.done());
    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(p.made, 0);
}

// A read the loop has already taken back has nothing out to abort: asked, it is not
// cancelled, and the co_await that comes after the request still ends with what the read
// brought -- the read was over before the request, and is not undone by it.
TEST(CancellationTest, AnOperationBackBeforeItsCoAwaitKeepsItsAnswerAfterTheRequest) {
    probe p;
    p.gate.set();

    std::byte buf[16]{};
    cancellation_source source;
    task<std::size_t> read = start_read(p, buf, source.token());
    sta_loop::run_until([&] { return read.done(); });

    source.cancel();

    EXPECT_EQ(p.canceled, 0);
    EXPECT_EQ(p.wrote, 1);

    task<std::size_t> joined = joins(read);

    EXPECT_TRUE(joined.done());
    EXPECT_EQ(joined.result(), 16u);
}

// One that has done its work and is on its way back when the request comes is cancelled -- it
// is still out -- and has nothing left to abort: it answers what it brought.
TEST(CancellationTest, AnOperationDoneOnTheWorkerKeepsItsAnswerWhenCancelledOnItsWayBack) {
    probe p;
    p.gate.set();

    std::byte buf[16]{};
    cancellation_source source;
    task<std::size_t> read = start_read(p, buf, source.token());

    // Written, so the body is over; the loop has not taken the read back.
    while (p.wrote == 0) std::this_thread::yield();
    EXPECT_FALSE(read.done());

    source.cancel();
    EXPECT_EQ(p.canceled, 1);

    sta_loop::run_until([&] { return read.done(); });

    EXPECT_EQ(read.result(), 16u);
    EXPECT_EQ(p.wrote, 1);
}

// What the form with a token returns is a task like any other: kept and awaited later, it
// gives what the read made, for as long as the source -- the right to the work -- lives.
TEST(CancellationTest, AKeptOperationUnderATokenIsAwaitedLater) {
    std::coroutine_handle<> slot;

    {
        probe p;
        p.gate.set();

        cancellation_source source;
        task<std::size_t> chain = keeps_the_read(p, source.token(), slot);

        ASSERT_TRUE(slot);
        slot.resume();
        sta_loop::run_until([&] { return chain.done(); });

        EXPECT_EQ(chain.result(), 16u);
        EXPECT_EQ(p.canceled, 0);
    }

    // The source gone while the read is kept is the read cancelled: the co_await that comes
    // later waits for it to come back and ends with the cancellation.
    {
        probe p;

        nullable<cancellation_source> source(std::in_place);
        task<std::size_t> chain = keeps_the_read(p, source->token(), slot);

        p.started.wait();
        source.reset();
        EXPECT_EQ(p.canceled, 1) << "the source went and the read it held was not cancelled";

        ASSERT_TRUE(slot);
        slot.resume();
        sta_loop::run_until([&] { return chain.done(); });

        EXPECT_TRUE(ends_cancelled(chain));
        EXPECT_EQ(p.wrote, 0);
        EXPECT_EQ(p.alive, 0);
    }

    // Asked while it is kept, it is cancelled at once -- it is registered with the token from the
    // moment it is made, awaited or not -- and the co_await that comes later waits for it
    // to come back and ends with the cancellation.
    probe p;
    cancellation_source source;
    task<std::size_t> chain = keeps_the_read(p, source.token(), slot);

    p.started.wait();
    source.cancel();
    EXPECT_EQ(p.canceled, 1);

    slot.resume();
    EXPECT_EQ(p.canceled, 1);

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

        cancellation_source source;
        task<std::size_t> chain = moves_the_read(p, source.token());

        sta_loop::run_until([&] { return chain.done(); });
        EXPECT_EQ(chain.result(), 16u);
    }

    probe p;
    cancellation_source source;
    task<std::size_t> chain = moves_the_read(p, source.token());

    p.started.wait();
    source.cancel();
    EXPECT_EQ(p.canceled, 1);

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
        cancellation_source source;
        {
            task<std::size_t> chain = keeps_the_read(p, source.token(), slot);
            p.started.wait();
        }

        EXPECT_EQ(p.canceled, 1) << "the read was given up";
        sta_loop::run_until([&] { return p.alive == 0; });

        source.cancel();
        EXPECT_EQ(p.canceled, 1);
    }

    // The source going first cancels the kept read, and empties the event: the frame destroyed
    // after it gives up a read that is registered nowhere any more, and asks it to stop no second
    // time. The read's token, the last holder of the state, answers that it is cancelled.
    probe p;
    std::coroutine_handle<> slot;
    nullable<cancellation_source> source(std::in_place);
    {
        task<std::size_t> chain = keeps_the_read(p, source->token(), slot);

        p.started.wait();
        slot.resume();

        const cancellation_token kept = source->token();
        source.reset();

        EXPECT_EQ(p.canceled, 1) << "the source went and the read it held was not cancelled";
        EXPECT_TRUE(kept.is_canceled()) << "a token outliving its source is not cancelled";
        EXPECT_FALSE(static_cast<bool>(detail::cancellation_state_of(kept)->event()))
            << "the cancelled read is still registered with the event";
    }

    EXPECT_EQ(p.canceled, 1);
    sta_loop::run_until([&] { return p.alive == 0; });
}

// Many under one token, leaving in any order: the one in the middle of the event comes back
// and goes first, and the request reaches the two still registered with the token -- the
// first, back and kept, keeps its answer, and only the last, still out, is cancelled.
TEST(CancellationTest, ManyUnderOneTokenLeaveInAnyOrderAndTheRestAreCancelled) {
    probe first, middle, last;
    middle.gate.set();

    std::byte a[16]{}, b[16]{}, c[16]{};
    cancellation_source source;

    task<std::size_t> one = start_read(first, a, source.token());
    task<std::size_t> two = start_read(middle, b, source.token());
    task<std::size_t> three = start_read(last, c, source.token());

    // The worker takes them in turn: let the first through only once the middle one would
    // be next, so that it is the middle one that comes back and goes.
    first.gate.set();
    sta_loop::run_until([&] { return two.done(); });
    EXPECT_EQ(two.result(), 16u);
    two = task<std::size_t>(detail::canceled_task<std::size_t>());
    EXPECT_EQ(middle.alive, 0);

    source.cancel();

    EXPECT_EQ(first.canceled, 0);
    EXPECT_EQ(middle.canceled, 0);
    EXPECT_EQ(last.canceled, 1);

    sta_loop::run_until([&] { return one.done() && three.done(); });

    // The first read came back before the request and keeps what it brought; the last was
    // aborted.
    EXPECT_EQ(one.result(), 16u);
    EXPECT_TRUE(ends_cancelled(three));
    EXPECT_EQ(last.wrote, 0);
}

// ---- The loop's calls under a token, for the operations of this module -----------------

/// A body on the worker, kept behind a read that holds the worker, counting its runs.
task<int> counts_on_the_worker(int& runs, cancellation_token token) {
    co_return co_await sta_loop::async_call([&runs] { return ++runs; }, std::move(token));
}

// The lambda forms take the token as the class form does: one not reached by the worker
// never runs, and the co_await ends with the cancellation once it is back.
TEST(CancellationTest, ALambdaOnTheWorkerCancelledBeforeItIsReachedNeverRuns) {
    probe ahead;
    std::byte ahead_buf[16]{};
    auto holds_the_worker = start_read(ahead, ahead_buf);
    ahead.started.wait();

    int runs = 0;
    cancellation_source source;
    task<int> counted = counts_on_the_worker(runs, source.token());

    source.cancel();
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
    task<int> refused = counts_on_the_worker(runs, source.token());
    EXPECT_TRUE(refused.done());
    EXPECT_TRUE(ends_cancelled(refused));
    EXPECT_EQ(runs, 1);
}

// Under a token cancelled already the call answers the cancellation as it returns, and answers
// it without throwing: the answer is the one exception made once and shared, so two calls carry
// the very same object -- one thrown inside each call would be an exception of its own. The
// cancellation is thrown only where it is read.
TEST(CancellationTest, UnderACancelledTokenTheCallAnswersWithoutThrowing) {
    cancellation_source source;
    source.cancel();

    int runs = 0;
    task<int> refused = sta_loop::async_call([&runs] { return ++runs; }, source.token());

    ASSERT_TRUE(refused.done());
    EXPECT_FALSE(refused.has_value());
    ASSERT_TRUE(refused.has_exception());
    EXPECT_THROW(std::rethrow_exception(refused.get_exception_ptr()), operation_canceled_exception);
    EXPECT_THROW((void)refused.result(), operation_canceled_exception);

    task<int> again = sta_loop::async_call([&runs] { return ++runs; }, source.token());
    ASSERT_TRUE(again.has_exception());
    EXPECT_EQ(refused.get_exception_ptr(), again.get_exception_ptr());

    // A body that answers nothing answers the cancellation the same way.
    task<> nothing = sta_loop::async_call([&runs] { ++runs; }, source.token());
    ASSERT_TRUE(nothing.done());
    EXPECT_EQ(nothing.get_exception_ptr(), refused.get_exception_ptr());
    EXPECT_THROW(nothing.result(), operation_canceled_exception);

    // Dropped unread, the task lets its answer go with it.
    (void)sta_loop::async_call([&runs] { return ++runs; }, source.token());

    EXPECT_EQ(runs, 0);
}

/// Awaits a call under `token` and says whether the co_await threw the cancellation.
task<bool> awaits_the_cancellation(cancellation_token token) {
    try {
        (void)co_await sta_loop::async_call([] { return 1; }, std::move(token));
    } catch (const operation_canceled_exception&) {
        co_return true;
    }

    co_return false;
}

// The co_await of a task body finds the answer there and does not suspend: it throws the
// cancellation at once, and the body ends before the call that started it returns.
TEST(CancellationTest, UnderACancelledTokenTheCoAwaitThrowsTheCancellationAtOnce) {
    cancellation_source source;
    source.cancel();

    task<bool> body = awaits_the_cancellation(source.token());

    ASSERT_TRUE(body.done());
    EXPECT_TRUE(body.result());
}

// An orphanable body under a live token runs and answers; cancelled while it waits for the
// worker, it never runs, and the co_await ends with the cancellation without waiting for it;
// under a token cancelled already it is not made.
TEST(CancellationTest, TheOrphanableFormTakesTheTokenToo) {
    cancellation_source source;

    task<int> ran = sta_loop::async_call(orphanable, [] { return 5; }, source.token());
    sta_loop::run_until([&] { return ran.done(); });
    EXPECT_EQ(ran.result(), 5);

    probe ahead;
    std::byte ahead_buf[16]{};
    auto holds_the_worker = start_read(ahead, ahead_buf);
    ahead.started.wait();

    std::atomic<int> runs{0};
    const orphans_drained_on_exit drained(ahead.gate);
    task<int> canceled = sta_loop::async_call(orphanable, [&runs] { return ++runs; }, source.token());

    source.cancel();
    ahead.gate.set();
    sta_loop::run_until([&] { return canceled.done() && holds_the_worker.done(); });

    EXPECT_TRUE(ends_cancelled(canceled));

    // The orphan, given up before the worker reached it, comes back without having run.
    sta_loop::run_until([] { return sta_loop::outstanding() == 0; });
    EXPECT_EQ(runs, 0);

    bool made = false;
    task<int> not_made =
        sta_loop::async_call(orphanable, [&made] { return made = true, 6; }, source.token());
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

// What comes back is the answer as the operation made it, whenever the request came: one
// cancelled after its body ran answers its value, one cancelled before the worker reached it
// never runs and answers the cancellation the worker wrote for it, and one never cancelled its
// value. Taken back with nobody waiting, each is there for the co_await still to come.
TEST(CancellationTest, AnOperationCancelledWhileOutComesBackWithWhatHappenedToIt) {
    using op = detail::cancelable_operation<carried_by_hand>;

    cancellation_source source;

    auto* const done_first = new op(source.token(), 3);
    task<int> done_first_task(std::unique_ptr<async_op_t<int>>{done_first});

    auto* const not_reached = new op(source.token(), 5);
    task<int> not_reached_task(std::unique_ptr<async_op_t<int>>{not_reached});

    EXPECT_TRUE(done_first->packaged_execute());

    source.cancel();

    EXPECT_TRUE(not_reached->packaged_execute());
    EXPECT_EQ(not_reached->runs, 0) << "an operation cancelled before the worker reached it ran";

    done_first->complete();
    not_reached->complete();

    EXPECT_TRUE(done_first_task.done());
    EXPECT_EQ(done_first_task.result(), 3);

    EXPECT_TRUE(not_reached_task.done());
    EXPECT_TRUE(ends_cancelled(not_reached_task));

    cancellation_source fresh;
    auto* const plain = new op(fresh.token(), 4);
    task<int> plain_task(std::unique_ptr<async_op_t<int>>{plain});

    EXPECT_TRUE(plain->packaged_execute());
    plain->complete();

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
    cancellation_source source;
    const orphans_drained_on_exit drained(gate);

    {
        task<int> dropped = sta_loop::async_call(
            orphanable,
            [&started, &gate, &finished, keep = counted_capture(&alive)] {
                started.set();
                gate.wait();
                finished = true;
                return 1;
            },
            source.token());

        started.wait();
    }

    source.cancel();
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
    } catch (const last_error& failure) {
        return std::format("system error {}", failure.code().value());
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

// A lambda on the worker cannot be aborted: cancelled while it runs, it finishes, and the
// co_await ends with what it made.
TEST(CancellationTest, ABodyOnTheWorkerCancelledWhileItRunsAnswersWhatItMade) {
    hevent started{true}, gate{true};
    cancellation_source source;

    task<int> made = sta_loop::async_call(
        [&started, &gate] {
            started.set();
            gate.wait();
            return 7;
        },
        source.token());

    started.wait();
    source.cancel();
    gate.set();

    sta_loop::run_until([&] { return made.done(); });

    EXPECT_EQ(answer_of(made), "value 7");
}

// An orphan cancelled while it stands in a call is let go of at once: the co_await answers
// the cancellation whatever the call then does -- fails as a call cancelled under it does, or
// finishes first -- and the body, its stop requested, comes back alone.
TEST(CancellationTest, AnOrphanCancelledWhileItRunsAnswersTheCancellationWhateverItsCallDoes) {
    for (const bool canceled : {true, false}) {
        SCOPED_TRACE(canceled ? "its call cancelled" : "its call done first");

        hevent started{true}, gate{true};
        bool saw_stop_requested = false;
        cancellation_source source;
        const orphans_drained_on_exit drained(gate);

        task<int> reading = sta_loop::async_call(
            orphanable,
            [&started, &gate, &saw_stop_requested, canceled](const orphan_state& state) -> int {
                started.set();

                // Stands for the call to the system the cancelling aborts.
                gate.wait();

                saw_stop_requested = state.stop_requested();

                if (canceled) throw last_error("ReadFile", ERROR_OPERATION_ABORTED);

                return 1;
            },
            source.token());

        started.wait();
        source.cancel();

        sta_loop::run_until([&] { return reading.done(); });
        EXPECT_EQ(answer_of(reading), "cancelled");

        gate.set();
        sta_loop::run_until([] { return sta_loop::outstanding() == 0; });

        EXPECT_TRUE(saw_stop_requested);
    }
}

// A failure of the body's own stays its answer, request or no request, for the lambda on the
// worker, which is waited for; the orphan, let go of at the request, answers the cancellation
// and its failure goes with it.
TEST(CancellationTest, AFailureOfTheBodysOwnStaysTheAnswerOnTheWorkerOnly) {
    {
        hevent started{true}, gate{true};
        cancellation_source source;

        task<int> failing = sta_loop::async_call(
            [&started, &gate]() -> int {
                started.set();
                gate.wait();
                throw std::runtime_error("its own");
            },
            source.token());

        started.wait();
        source.cancel();
        gate.set();

        sta_loop::run_until([&] { return failing.done(); });

        EXPECT_EQ(answer_of(failing), "its own");
    }

    hevent started{true}, gate{true};
    cancellation_source source;
    const orphans_drained_on_exit drained(gate);

    task<int> refused = sta_loop::async_call(
        orphanable,
        [&started, &gate]() -> int {
            started.set();
            gate.wait();
            throw last_error("CreateFileW", ERROR_ACCESS_DENIED);
        },
        source.token());

    started.wait();
    source.cancel();
    gate.set();

    sta_loop::run_until([&] { return refused.done(); });
    EXPECT_EQ(answer_of(refused), "cancelled");

    sta_loop::run_until([] { return sta_loop::outstanding() == 0; });
}

// An orphan cancelled is let go of at once, and what it makes is let go of as any orphan
// given up lets go of it -- on the thread that makes it, as soon as it is made, and not when
// the loop meets the operation in the return channel -- whether or not its task is dropped
// before the loop's turn comes.
TEST(CancellationTest, AnOrphanCancelledAndThenGivenUpLetsGoOfWhatItMakesAtOnce) {
    hevent started{true}, gate{true};
    std::atomic<int> alive{0};
    std::atomic<bool> made{false};
    cancellation_source source;
    const orphans_drained_on_exit drained(gate);

    {
        auto making = sta_loop::async_call(
            orphanable,
            [&started, &gate, &alive, &made] {
                started.set();
                gate.wait();

                counted_result result(alive);
                made = true;
                return result;
            },
            source.token());

        started.wait();
        source.cancel();
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
// one, and is registered nowhere.
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

// ---- A source under a parent token -----------------------------------------------------

namespace {

/// Whether anybody is registered with the event of a token's state: an operation, a wait, a source
/// made under the token.
bool anybody_registered(const cancellation_token& token) {
    return static_cast<bool>(detail::cancellation_state_of(token)->event());
}

/// The thread's queue, as a wait of wxl.ui sees it: a cancelled wait hands its coroutine here
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
/// came -- or, cancelled, on the queue's turn.
struct queued_awaiter {
    turns* queue;
    std::coroutine_handle<>* slot;
    bool canceled = false;

    bool await_ready() const noexcept { return canceled; }
    void await_suspend(std::coroutine_handle<> here) noexcept { *slot = here; }
    void await_resume() const noexcept {}

    void cancel() noexcept {
        canceled = true;
        if (*slot) queue->push_back(std::exchange(*slot, {}));
    }
};

/// A wait of wxl under a token over that awaiter: what the event waits of wxl.ui are made of.
class queued_wait : public detail::cancelable_wait<queued_wait>
{
    friend detail::cancelable_wait<queued_wait>;

public:
    queued_wait(turns& queue, std::coroutine_handle<>& slot, cancellation_token token) noexcept
        : token_(std::move(token)), awaiter_{.queue = &queue, .slot = &slot} {}

    queued_wait(const queued_wait&) = delete;
    queued_wait& operator=(const queued_wait&) = delete;

    ~queued_wait() { this->unregister_from_token(); }

private:
    queued_awaiter& awaiter() noexcept { return awaiter_; }

    detail::cancellation_state* state() const noexcept {
        return detail::cancellation_state_of(token_);
    }

    cancellation_token token_;
    queued_awaiter awaiter_;
};

task<> waits_its_turn(turns& queue, std::coroutine_handle<>& slot, cancellation_token token,
                      std::string& ended) {
    try {
        co_await queued_wait{queue, slot, std::move(token)};
        ended = "came";
    } catch (const operation_canceled_exception&) {
        ended = "cancelled";
    }
}

}  // namespace

// Cancelling the parent cancels the child, and what is registered with the child is aborted as
// if the child had been cancelled itself: cancelled inside the parent's cancel(), resumed by
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
    EXPECT_EQ(p.canceled, 1);
    EXPECT_FALSE(chain.done()) << "cancel() resumed the chain itself";

    sta_loop::run_until([&] { return chain.done(); });

    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(p.wrote, 0);
    EXPECT_EQ(p.alive, 0);
}

// Cancelling the child reaches its own and not the parent's. The child stays registered with
// the parent's event until it goes, and the parent's cancel() then cancels it a second time,
// which cancels nobody.
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
        EXPECT_EQ(under_child.canceled, 1);
        EXPECT_EQ(under_parent.canceled, 0);

        under_parent.gate.set();
        sta_loop::run_until([&] { return parents.done() && childs.done(); });

        EXPECT_EQ(parents.result(), 16u);
        EXPECT_TRUE(ends_cancelled(childs));
        EXPECT_EQ(under_child.ran, 0);
    }

    EXPECT_TRUE(anybody_registered(parent.token())) << "the child left the parent's event early";

    parent.cancel();

    EXPECT_TRUE(child.is_canceled());
    EXPECT_EQ(under_child.canceled, 1);
    EXPECT_FALSE(anybody_registered(child.token()));

    probe late;
    task<std::size_t> refused = top(late, child.token());
    EXPECT_TRUE(ends_cancelled(refused));
    EXPECT_EQ(late.made, 0);
}

// Under a parent cancelled already the child is born cancelled and registered nowhere; under a
// token of nobody it is a source like any other.
TEST(CancellationTest, UnderACancelledParentTheChildIsBornCancelled) {
    cancellation_source parent;
    parent.cancel();

    cancellation_source child(parent.token());

    EXPECT_TRUE(child.is_canceled());
    EXPECT_FALSE(anybody_registered(parent.token()))
        << "the child is registered with an event that has fired";

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
// event once its last token has gone, the source or not -- and the parent's cancel() reaches
// nothing that is gone. The child's source going is the child cancelled: a token of it held
// past that answers so.
TEST(CancellationTest, AChildGoneBeforeTheParentIsCancelledLeavesItsEvent) {
    cancellation_source parent;

    {
        cancellation_source child(parent.token());
        EXPECT_TRUE(anybody_registered(parent.token()));
    }
    EXPECT_FALSE(anybody_registered(parent.token()));

    {
        cancellation_source child(parent.token());
        child.cancel();
    }
    EXPECT_FALSE(anybody_registered(parent.token()));

    std::unique_ptr<wxl::test::sta_box<cancellation_token>> token;
    {
        cancellation_source child(parent.token());
        token = std::make_unique<wxl::test::sta_box<cancellation_token>>(child.token());
    }
    EXPECT_TRUE(token->value.is_canceled()) << "the child's source went and its token is not cancelled";
    EXPECT_FALSE(parent.is_canceled());
    EXPECT_TRUE(anybody_registered(parent.token())) << "the child left while its token was held";
    token.reset();
    EXPECT_FALSE(anybody_registered(parent.token()));

    parent.cancel();
    EXPECT_TRUE(parent.is_canceled());
}

// Three generations: cancelling the middle one cancels the youngest and not the eldest, and
// cancelling the eldest reaches the youngest through the middle one -- cancelled inside a
// fire inside a fire, resuming nobody.
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
    EXPECT_EQ(e.canceled + m.canceled + y.canceled, 3);
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

// The parent's source gone is the child cancelled, and a token of either, kept past both sources,
// still answers: the states live as long as somebody holds them, the parent's held by the child's.
// That nothing is left once the last holder goes -- that the parent holds nothing of the child, so
// there is no cycle -- is the code's, not this test's to show: the child's state is registered with
// the parent's event by a plain pointer, and only the child holds the parent.
TEST(CancellationTest, AChildHoldsItsParentsStateAfterTheParentsSourceGoes) {
    {
        nullable<cancellation_source> parent(std::in_place);
        nullable<cancellation_source> child(std::in_place, parent->token());
        const cancellation_token parents_token = parent->token();
        const cancellation_token childs_token = child->token();

        parent.reset();
        EXPECT_TRUE(child->is_canceled()) << "the parent's source went and the child was not cancelled";

        probe p;
        task<std::size_t> chain = top(p, child->token());
        EXPECT_TRUE(ends_cancelled(chain));
        EXPECT_EQ(p.made, 0);

        child.reset();
        EXPECT_TRUE(parents_token.is_canceled());
        EXPECT_TRUE(childs_token.is_canceled());
    }

    // What is registered with the child when the parent's source goes is cancelled, and comes back
    // with the cancellation.
    {
        nullable<cancellation_source> parent(std::in_place);
        cancellation_source child(parent->token());

        probe p;
        task<std::size_t> chain = top(p, child.token());
        p.started.wait();

        parent.reset();
        EXPECT_EQ(p.canceled, 1);

        sta_loop::run_until([&] { return chain.done(); });
        EXPECT_TRUE(ends_cancelled(chain));
        EXPECT_EQ(p.wrote, 0);
        EXPECT_EQ(p.alive, 0);
    }
}

// Many children under one parent, going front to back and back to front: each takes its own
// callback out of the parent's event and no other, and the parent's cancel() reaches those
// still there -- one cancelled by itself already among them.
TEST(CancellationTest, ManyChildrenUnderOneParentGoInEitherOrder) {
    for (const bool backwards : {false, true}) {
        SCOPED_TRACE(backwards ? "back to front" : "front to back");

        cancellation_source parent;
        std::vector<nullable<cancellation_source>> children(8);

        const std::size_t n = children.size();
        const auto nth = [&](std::size_t k) -> auto& {
            return children[backwards ? n - 1 - k : k];
        };

        for (auto& child : children) child.emplace(parent.token());
        for (std::size_t k = 0; k < n; ++k) {
            EXPECT_TRUE(anybody_registered(parent.token()));
            nth(k).reset();
        }
        EXPECT_FALSE(anybody_registered(parent.token()));

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

// A wait of wxl under the child's token: without a request it ends when its event comes;
// cancelled when the parent is cancelled, it is handed to the queue inside the parent's
// cancel() and ends with the cancellation on the queue's turn; begun under the child
// cancelled, it ends at once without suspending.
TEST(CancellationTest, AWaitUnderTheChildIsCancelledWithTheParent) {
    turns queue;
    std::coroutine_handle<> slot;
    cancellation_source parent;
    cancellation_source child(parent.token());

    std::string came;
    task<> first = waits_its_turn(queue, slot, child.token(), came);
    ASSERT_TRUE(slot);
    std::exchange(slot, {}).resume();
    EXPECT_EQ(came, "came");

    std::string canceled;
    task<> second = waits_its_turn(queue, slot, child.token(), canceled);
    EXPECT_TRUE(anybody_registered(child.token()));

    parent.cancel();

    EXPECT_FALSE(second.done()) << "cancel() resumed the wait itself";
    ASSERT_EQ(queue.size(), 1u);
    take_turns(queue);
    EXPECT_EQ(canceled, "cancelled");

    std::string at_once;
    task<> third = waits_its_turn(queue, slot, child.token(), at_once);
    EXPECT_TRUE(third.done());
    EXPECT_EQ(at_once, "cancelled");
    EXPECT_TRUE(queue.empty());
}

// What the child stands on: cancelled inside the parent's fire, it cancels its own -- an
// operation, a wait, a child of its own -- and destroys nobody, while its callback lies
// in the event the parent fires. Its sources then go before what is registered with them, and
// each state, going last, finds its parent's event emptied by the fire and takes nothing
// out of it.
TEST(CancellationTest, TheParentsFireCancelsTheChildsOwnAndDestroysNobody) {
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

    nullable<cancellation_source> child(std::in_place, parent.token());
    nullable<cancellation_source> grandchild(std::in_place, child->token());

    task<std::size_t> childs = top(under_child, child->token());
    task<> waits = waits_its_turn(queue, slot, child->token(), ended);
    task<std::size_t> grandchilds = top(under_grandchild, grandchild->token());
    task<std::size_t> last = top(after, parent.token());

    parent.cancel();

    EXPECT_EQ(before.canceled + under_child.canceled + under_grandchild.canceled + after.canceled,
              4);
    EXPECT_EQ(queue.size(), 1u);
    EXPECT_FALSE(first.done() || childs.done() || waits.done() || grandchilds.done() ||
                 last.done())
        << "cancel() resumed somebody";
    EXPECT_FALSE(anybody_registered(parent.token()));

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

// ---- The source lives as long as the right to the work ---------------------------------

// The source gone is the work cancelled: what is registered with its token is aborted as by
// cancel(), and the chain ends by itself with the cancellation.
TEST(CancellationTest, ASourceThatGoesCancelsWhatIsRegisteredWithItsToken) {
    probe p;
    nullable<cancellation_source> source(std::in_place);
    task<std::size_t> chain = top(p, source->token());

    p.started.wait();
    source.reset();

    EXPECT_EQ(p.canceled, 1);
    EXPECT_FALSE(chain.done()) << "the destructor resumed the chain itself";

    sta_loop::run_until([&] { return chain.done(); });

    EXPECT_TRUE(ends_cancelled(chain));
    EXPECT_EQ(p.wrote, 0);
    EXPECT_EQ(p.alive, 0);
}

// ...and a source made under its token, with what is registered with that one.
TEST(CancellationTest, ASourceThatGoesCancelsTheSourcesUnderIt) {
    probe p;
    nullable<cancellation_source> parent(std::in_place);
    cancellation_source child(parent->token());
    task<std::size_t> chain = top(p, child.token());

    p.started.wait();
    parent.reset();

    EXPECT_TRUE(child.is_canceled());
    EXPECT_EQ(p.canceled, 1);

    sta_loop::run_until([&] { return chain.done(); });
    EXPECT_TRUE(ends_cancelled(chain));
}

// Moving hands the right on and cancels nothing: the source moved from is empty, and its
// going does nothing; the work is registered with the source it moved to, and goes with that one.
TEST(CancellationTest, AMovedSourceCancelsNothingAndTheOneMovedFromDoesNothing) {
    probe p;
    p.gate.set();

    nullable<cancellation_source> first(std::in_place);
    std::coroutine_handle<> slot;
    task<std::size_t> chain = keeps_the_read(p, first->token(), slot);

    cancellation_source second(std::move(*first));
    first.reset();

    EXPECT_FALSE(second.is_canceled());
    EXPECT_EQ(p.canceled, 0) << "moving the source, or the empty one going, cancelled the read";

    ASSERT_TRUE(slot);
    slot.resume();
    sta_loop::run_until([&] { return chain.done(); });

    EXPECT_EQ(chain.result(), 16u);
    EXPECT_EQ(p.canceled, 0);
}

// Assigned over, a source cancels the work it held, as its destructor would, and takes over
// the other's without cancelling it.
TEST(CancellationTest, ASourceAssignedOverCancelsTheWorkItHeld) {
    probe held, taken;
    std::byte buf[16]{};

    // The read under the source assigned over keeps the worker; the other waits behind it.
    cancellation_source field;
    task<std::size_t> overwritten = start_read(held, buf, field.token());
    held.started.wait();

    cancellation_source next;
    task<std::size_t> kept = top(taken, next.token());

    field = std::move(next);

    EXPECT_EQ(held.canceled, 1) << "the work of the source assigned over was not cancelled";
    EXPECT_EQ(taken.canceled, 0) << "the work taken over was cancelled";
    EXPECT_FALSE(field.is_canceled());

    taken.gate.set();
    sta_loop::run_until([&] { return overwritten.done() && kept.done(); });

    EXPECT_TRUE(ends_cancelled(overwritten));
    EXPECT_EQ(kept.result(), 16u);

    // A source moved onto itself stays as it is.
    cancellation_source& same = field;
    field = std::move(same);
    EXPECT_FALSE(field.is_canceled());
}

// A source cancelled already has nobody registered: its going asks nothing of anybody again --
// neither what it cancelled, nor a child that cancelled itself first and is still registered with
// its event, which the parent's going reaches.
TEST(CancellationTest, ASourceCancelledAlreadyDoesNothingAsItGoes) {
    probe p;
    task<std::size_t> chain;
    {
        cancellation_source source;
        chain = top(p, source.token());
        p.started.wait();

        source.cancel();
        EXPECT_EQ(p.canceled, 1);
    }
    EXPECT_EQ(p.canceled, 1) << "the source cancelled already asked again as it went";

    sta_loop::run_until([&] { return chain.done(); });
    EXPECT_TRUE(ends_cancelled(chain));

    probe under_child;
    nullable<cancellation_source> parent(std::in_place);
    cancellation_source child(parent->token());
    task<std::size_t> childs = top(under_child, child.token());
    under_child.started.wait();

    child.cancel();
    EXPECT_EQ(under_child.canceled, 1);
    EXPECT_TRUE(anybody_registered(parent->token()))
        << "the child cancelled by itself left its parent's event";

    parent.reset();
    EXPECT_EQ(under_child.canceled, 1) << "the parent's going asked the cancelled child again";

    sta_loop::run_until([&] { return childs.done(); });
    EXPECT_TRUE(ends_cancelled(childs));
}

namespace {

/// Hands part of its work to another owner, under a source of its own in its frame, and fails
/// before that work is done.
task<> hands_off_and_fails(probe& p, cancellation_token token, std::vector<task<>>& others,
                           bool& handed_off_canceled) {
    cancellation_source local(token);

    others.push_back(cancelable_handed_off(p, local.token(), handed_off_canceled));

    co_await sta_loop::call_here([] { throw std::runtime_error("failed"); });
}

}  // namespace

// A source that is a local of a frame is the right to the work it started: the frame unwound
// by an exception takes its subtree down with it -- work handed to another owner, whose read
// is registered with it, is cancelled and ends -- and the work above is not touched.
TEST(CancellationTest, AFrameUnwindingCancelsTheWorkOfItsOwnSource) {
    probe p;
    bool canceled = false;
    std::vector<task<>> others;
    cancellation_source outer;

    task<> work = hands_off_and_fails(p, outer.token(), others, canceled);

    ASSERT_TRUE(work.done());
    EXPECT_THROW(work.result(), std::runtime_error);
    EXPECT_EQ(p.canceled, 1) << "the frame went and the work under its source was not cancelled";
    EXPECT_FALSE(outer.is_canceled());

    ASSERT_EQ(others.size(), 1u);
    sta_loop::run_until([&] { return others.front().done(); });

    EXPECT_TRUE(canceled);
    EXPECT_EQ(p.wrote, 0);
    EXPECT_EQ(p.alive, 0);
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
void cancel_there(cancellation_source& source) { source.cancel(); }

void cancel_from_another_thread() {
    report_failures_to_stderr();

    cancellation_source source;
    std::thread([&source] { cancel_there(source); }).join();
}

constexpr std::uint_least32_t line_of_the_foreign_child = std::source_location::current().line() + 1;
void make_a_child_there(cancellation_source& parent) { cancellation_source child(parent.token()); }

void make_a_child_on_another_thread() {
    report_failures_to_stderr();

    cancellation_source parent;
    std::thread([&parent] { make_a_child_there(parent); }).join();
}

void ask_a_moved_from_source() {
    report_failures_to_stderr();

    cancellation_source source;
    cancellation_source taken(std::move(source));
    (void)source.token();
}

class CancellationDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!detail::checked) GTEST_SKIP() << "built without coroutine checks";
    }
};

/// A strict build's report names the line of the caller.
class CancellationStrictDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!detail::strict) GTEST_SKIP() << "built without STRICT_CORO";
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

TEST_F(CancellationDeathTest, AMovedFromSourceIsNotAsked) {
    EXPECT_DEATH(ask_a_moved_from_source(), "cancellation_source: empty -- moved from");
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

// One source is one right to the work: it moves and is not copied.
static_assert(!std::is_copy_constructible_v<cancellation_source> &&
              !std::is_copy_assignable_v<cancellation_source>);
static_assert(std::is_nothrow_move_constructible_v<cancellation_source> &&
              std::is_nothrow_move_assignable_v<cancellation_source>);

// A token is not a source: a child is made by name, from its parent's token.
static_assert(std::is_constructible_v<cancellation_source, cancellation_token>);
static_assert(!std::is_convertible_v<cancellation_token, cancellation_source>);

// The form with a token is the same task: the token and the place in its event live in the
// operation, and the plain form has neither.
static_assert(sizeof(task<std::size_t>) == sizeof(void*));
static_assert(std::is_same_v<decltype(start_read(std::declval<probe&>(), {}, cancellation_token{})),
                             task<std::size_t>>);
static_assert(
    std::is_same_v<decltype(sta_loop::async_call(std::declval<int (*)()>(), cancellation_token{})),
                   task<int>>);
static_assert(sizeof(detail::cancelable_operation<gated_read>) ==
              sizeof(gated_read) + sizeof(cancellation_token) + 3 * sizeof(void*));

// The loop's calls that send a body take a token, last; the call that answers in place takes
// none -- its body has run before there is anything to cancel.
namespace {

template <class Fn>
concept answers_in_place_under_a_token =
    requires(Fn fn, cancellation_token token) { sta_loop::call_here(fn, token); };

}  // namespace

static_assert(!answers_in_place_under_a_token<int (*)()>);
static_assert(std::is_same_v<decltype(sta_loop::async_call(orphanable, std::declval<int (*)()>(),
                                                           cancellation_token{})),
                             task<int>>);
static_assert(std::is_same_v<decltype(sta_loop::async_call(
                                 orphanable, std::declval<int (*)(const orphan_state&)>(),
                                 cancellation_token{})),
                             task<int>>);

