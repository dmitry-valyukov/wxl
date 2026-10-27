// A semaphore the coroutines of one thread wait for without blocking it, on either loop: built
// into both binaries, since the waiter handed a permit is resumed by a turn the loop owes, which
// under the dispatcher queue is a drain posted to it. A free permit is taken without suspending;
// the waiters are served first come, first served, each resumed on a later turn and never inside
// the release(); a permit handed over is nobody else's; and a wait ends by its token -- before it
// begins, while it stands in line, or after a permit was handed to it -- or by its frame being
// destroyed, with the permit passed on and nothing left in the loop.
//
// Each test starts with nothing out and leaves nothing out: a waiter owed a turn is counted among
// the outstanding, and a turn left in line would be one the loop takes into a dead frame.
#include <crtdbg.h>

#include <gtest/gtest.h>

#include "loop_environment.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

/// Takes a permit and keeps it, writing down that it got there.
task<> takes(async_semaphore& semaphore, std::vector<int>& order, int id,
             cancellation_token token) {
    co_await semaphore.acquire(std::move(token));
    order.push_back(id);
}

/// Waits for an answer from the worker and signals that it is there.
task<> produces(async_semaphore& ready, int& value) {
    value = co_await sta_loop::async_call([] { return 42; });
    ready.release();
}

/// Waits for the signal, and reads what it signals.
task<int> consumes(async_semaphore& ready, const int& value) {
    co_await ready.acquire(cancellation_token{});
    co_return value;
}

class AsyncSemaphoreTest : public ::testing::Test
{
protected:
    void SetUp() override { ASSERT_EQ(sta_loop::outstanding(), 0u); }

    void TearDown() override { EXPECT_EQ(sta_loop::outstanding(), 0u); }
};

}  // namespace

// A free permit is taken inside the co_await: the coroutine goes on past it before anything turns
// the loop.
TEST_F(AsyncSemaphoreTest, AFreePermitIsTakenWithoutSuspending) {
    async_semaphore semaphore(1);
    std::vector<int> order;

    task<> first = takes(semaphore, order, 1, cancellation_token{});

    EXPECT_TRUE(first.done()) << "the co_await suspended on a free permit";
    EXPECT_EQ(order, std::vector<int>{1});
    EXPECT_FALSE(semaphore.try_acquire()) << "the permit taken is still free";

    semaphore.release();
    EXPECT_TRUE(semaphore.try_acquire());
    semaphore.release();
}

// Three coroutines wait, and each permit given back goes to the first in line -- resumed on the
// loop's next turn, the ones behind it still waiting.
TEST_F(AsyncSemaphoreTest, WaitersAreServedInTheOrderTheyCame) {
    async_semaphore semaphore(0);
    std::vector<int> order;

    task<> first = takes(semaphore, order, 1, cancellation_token{});
    task<> second = takes(semaphore, order, 2, cancellation_token{});
    task<> third = takes(semaphore, order, 3, cancellation_token{});

    EXPECT_FALSE(first.done());
    EXPECT_FALSE(second.done());
    EXPECT_FALSE(third.done());

    semaphore.release();
    EXPECT_TRUE(order.empty()) << "a waiter was resumed inside the release";

    sta_loop::run_until(first);
    EXPECT_FALSE(second.done());
    EXPECT_FALSE(third.done());

    semaphore.release();
    sta_loop::run_until(second);
    EXPECT_FALSE(third.done());

    semaphore.release();
    sta_loop::run_until(third);

    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

// A permit given back while somebody waits is handed to the waiter: try_acquire() does not find it
// before the waiter's turn comes, and an acquire made meanwhile stands in line behind it.
TEST_F(AsyncSemaphoreTest, APermitHandedToAWaiterIsNotFreeForOthers) {
    async_semaphore semaphore(0);
    std::vector<int> order;

    task<> waiting = takes(semaphore, order, 1, cancellation_token{});

    semaphore.release();
    EXPECT_FALSE(semaphore.try_acquire()) << "the permit handed over was taken by try_acquire()";

    task<> later = takes(semaphore, order, 2, cancellation_token{});
    EXPECT_FALSE(later.done()) << "the permit handed over was taken by a later acquire";

    sta_loop::run_until(waiting);
    EXPECT_EQ(order, std::vector<int>{1});
    EXPECT_FALSE(later.done());

    semaphore.release();
    sta_loop::run_until(later);

    EXPECT_EQ(order, (std::vector<int>{1, 2}));
}

// Under a token cancelled already the co_await does not suspend, takes nothing, and ends with the
// cancellation at once: the free permit is still there.
TEST_F(AsyncSemaphoreTest, UnderATokenCancelledAlreadyTheAcquireThrowsAtOnce) {
    async_semaphore semaphore(1);
    cancellation_source source;
    std::vector<int> order;

    source.cancel();
    task<> refused = takes(semaphore, order, 1, source.token());

    ASSERT_TRUE(refused.done()) << "the acquire under a cancelled token suspended";
    EXPECT_THROW(refused.result(), operation_canceled_exception);
    EXPECT_TRUE(order.empty());

    EXPECT_TRUE(semaphore.try_acquire()) << "the cancelled acquire took the permit";
    EXPECT_FALSE(semaphore.try_acquire());
    semaphore.release();
}

// A waiter cancelled while it stands in line leaves it: it ends with the cancellation on a later
// turn, not inside the cancel, the count is untouched, and the others are served in their order.
TEST_F(AsyncSemaphoreTest, AWaiterCancelledInLineLeavesItAndTheOthersKeepTheirOrder) {
    async_semaphore semaphore(0);
    cancellation_source source;
    std::vector<int> order;

    task<> first = takes(semaphore, order, 1, cancellation_token{});
    task<> canceled = takes(semaphore, order, 2, source.token());
    task<> third = takes(semaphore, order, 3, cancellation_token{});

    source.cancel();
    EXPECT_FALSE(canceled.done()) << "the waiter was resumed inside the cancel";
    EXPECT_EQ(sta_loop::outstanding(), 1u) << "the cancelled waiter is owed a turn";

    EXPECT_THROW(sta_loop::run_until(canceled), operation_canceled_exception);
    EXPECT_TRUE(order.empty());
    EXPECT_FALSE(semaphore.try_acquire()) << "the cancellation changed the count";

    semaphore.release(2);
    sta_loop::run_until(first);
    sta_loop::run_until(third);

    EXPECT_EQ(order, (std::vector<int>{1, 3}));
}

// A waiter handed a permit and cancelled before its turn comes passes the permit on to the next in
// line, and ends with the cancellation on its own turn.
TEST_F(AsyncSemaphoreTest, CancelledAfterTheHandOverThePermitGoesToTheNextWaiter) {
    async_semaphore semaphore(0);
    cancellation_source source;
    std::vector<int> order;

    task<> canceled = takes(semaphore, order, 1, source.token());
    task<> next = takes(semaphore, order, 2, cancellation_token{});

    semaphore.release();
    source.cancel();

    EXPECT_THROW(sta_loop::run_until(canceled), operation_canceled_exception);
    sta_loop::run_until(next);

    EXPECT_EQ(order, std::vector<int>{2});
    EXPECT_FALSE(semaphore.try_acquire()) << "the permit passed on was counted twice";
}

// With nobody behind it, the permit a cancelled waiter was handed goes back to the count.
TEST_F(AsyncSemaphoreTest, CancelledAfterTheHandOverThePermitGoesBackToTheCount) {
    async_semaphore semaphore(0);
    cancellation_source source;
    std::vector<int> order;

    task<> canceled = takes(semaphore, order, 1, source.token());

    semaphore.release();
    source.cancel();

    EXPECT_THROW(sta_loop::run_until(canceled), operation_canceled_exception);
    EXPECT_TRUE(order.empty());

    EXPECT_TRUE(semaphore.try_acquire()) << "the permit was lost with the cancelled waiter";
    EXPECT_FALSE(semaphore.try_acquire());
}

// A frame destroyed while it stands in line -- its task reset by the owner -- takes itself out of
// the line: the permit given back later goes to the waiter behind it.
TEST_F(AsyncSemaphoreTest, AFrameDestroyedInLineLeavesIt) {
    async_semaphore semaphore(0);
    std::vector<int> order;

    task<> dropped = takes(semaphore, order, 1, cancellation_token{});
    task<> next = takes(semaphore, order, 2, cancellation_token{});

    dropped = {};
    EXPECT_EQ(sta_loop::outstanding(), 0u);

    semaphore.release();
    sta_loop::run_until(next);
    EXPECT_EQ(order, std::vector<int>{2});

    semaphore.release();
    EXPECT_TRUE(semaphore.try_acquire());
}

// A frame destroyed after a permit was handed to it, before its turn: the turn is taken out of
// line and the permit passed on -- to the next waiter, or back to the count.
TEST_F(AsyncSemaphoreTest, AFrameDestroyedAfterTheHandOverPassesThePermitOn) {
    async_semaphore semaphore(0);
    std::vector<int> order;

    task<> dropped = takes(semaphore, order, 1, cancellation_token{});
    task<> next = takes(semaphore, order, 2, cancellation_token{});

    semaphore.release();
    EXPECT_EQ(sta_loop::outstanding(), 1u) << "the waiter handed the permit is owed a turn";

    dropped = {};
    EXPECT_EQ(sta_loop::outstanding(), 1u) << "the turn is the next waiter's now";

    sta_loop::run_until(next);
    EXPECT_EQ(order, std::vector<int>{2});

    task<> alone = takes(semaphore, order, 3, cancellation_token{});
    semaphore.release();
    alone = {};
    EXPECT_EQ(sta_loop::outstanding(), 0u) << "the dropped waiter's turn was left in line";

    EXPECT_TRUE(semaphore.try_acquire()) << "the permit was lost with the dropped frame";
    EXPECT_FALSE(semaphore.try_acquire());
}

// A frame destroyed after its token cancelled it, before the turn that would end it: the turn is
// taken out of line, and the semaphore is as it was.
TEST_F(AsyncSemaphoreTest, AFrameDestroyedAfterItsCancellationLeavesNoTurn) {
    async_semaphore semaphore(0);
    cancellation_source source;
    std::vector<int> order;

    task<> dropped = takes(semaphore, order, 1, source.token());

    source.cancel();
    EXPECT_EQ(sta_loop::outstanding(), 1u);

    dropped = {};
    EXPECT_EQ(sta_loop::outstanding(), 0u);

    EXPECT_FALSE(semaphore.try_acquire());
    semaphore.release();
    EXPECT_TRUE(semaphore.try_acquire());
}

// release(n) hands n permits to the first n waiters, in their order, and the one behind them
// waits on.
TEST_F(AsyncSemaphoreTest, ReleasingSeveralPermitsServesAsManyWaitersInOrder) {
    async_semaphore semaphore(0);
    std::vector<int> order;

    task<> first = takes(semaphore, order, 1, cancellation_token{});
    task<> second = takes(semaphore, order, 2, cancellation_token{});
    task<> third = takes(semaphore, order, 3, cancellation_token{});
    task<> fourth = takes(semaphore, order, 4, cancellation_token{});

    semaphore.release(3);
    EXPECT_TRUE(order.empty());

    wait_until([&] { return first.done() && second.done() && third.done(); });
    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
    EXPECT_FALSE(fourth.done());

    semaphore.release();
    sta_loop::run_until(fourth);
    EXPECT_EQ(order, (std::vector<int>{1, 2, 3, 4}));
}

// release(n) with fewer waiters than permits hands one to each waiter and leaves the rest free:
// a free permit then stands beside a waiter whose turn has not come, and is nobody's but the next
// to ask.
TEST_F(AsyncSemaphoreTest, PermitsLeftOverByAReleaseGoToTheCount) {
    async_semaphore semaphore(0);
    std::vector<int> order;

    task<> waiting = takes(semaphore, order, 1, cancellation_token{});

    semaphore.release(3);

    EXPECT_TRUE(semaphore.try_acquire());
    EXPECT_TRUE(semaphore.try_acquire());
    EXPECT_FALSE(semaphore.try_acquire()) << "the permit handed to the waiter was taken";

    sta_loop::run_until(waiting);
    EXPECT_EQ(order, std::vector<int>{1});
}

// A semaphore with no permit is a signal: the consumer waits until the producer, resumed by an
// answer from the worker, releases it.
TEST_F(AsyncSemaphoreTest, ASemaphoreWithNoPermitSignals) {
    async_semaphore ready(0);
    int value = 0;

    task<int> consumer = consumes(ready, value);
    EXPECT_FALSE(consumer.done());

    task<> producer = produces(ready, value);

    EXPECT_EQ(sta_loop::run_until(consumer), 42);
    sta_loop::run_until(producer);
}

// A source destroyed with a waiter in line under its token cancels it on the way: the waiter ends
// with the cancellation on a later turn, and nothing is left in the loop.
TEST_F(AsyncSemaphoreTest, ASourceDestroyedEndsTheWaitUnderItsToken) {
    async_semaphore semaphore(0);
    std::vector<int> order;
    task<> canceled;

    {
        cancellation_source source;
        canceled = takes(semaphore, order, 1, source.token());
    }

    EXPECT_FALSE(canceled.done()) << "the waiter was resumed inside the source's destructor";
    EXPECT_THROW(sta_loop::run_until(canceled), operation_canceled_exception);
    EXPECT_TRUE(order.empty());
    EXPECT_FALSE(semaphore.try_acquire());
}

// What has no sound continuation stops the process in a build that checks coroutines: a permit
// given back over the maximum, a semaphore destroyed with a coroutine waiting on it, and a call
// from a thread other than the coroutines' -- assert in a Debug build, core::abort with the
// caller's line under STRICT_CORO.

namespace {

/// A failed check goes to stderr and ends the process without a dialog, so
/// that a death test can read why.
void report_failures_to_stderr() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

// clang-format off: the test reads the line of the call as its own line + 1
constexpr std::uint_least32_t line_of_the_release_over_max = std::source_location::current().line() + 1;
void release_once_more(async_semaphore& semaphore) { semaphore.release(); }
// clang-format on

void release_over_max() {
    report_failures_to_stderr();

    async_semaphore semaphore(1, 1);
    release_once_more(semaphore);
}

task<> waits_on(async_semaphore& semaphore) { co_await semaphore.acquire(cancellation_token{}); }

void destroy_with_a_waiter() {
    report_failures_to_stderr();

    auto semaphore = std::make_unique<async_semaphore>(0);
    task<> waiting = waits_on(*semaphore);
    semaphore.reset();
}

// clang-format off: the test reads the line of the call as its own line + 1
constexpr std::uint_least32_t line_of_the_release_over_a_handed_permit = std::source_location::current().line() + 1;
void release_once_more_there(async_semaphore& semaphore) { semaphore.release(); }
// clang-format on

/// The permit handed to the waiter counts against the maximum until its turn comes: given back a
/// second time before then, it would make two.
void release_over_a_handed_permit() {
    report_failures_to_stderr();

    async_semaphore semaphore(0, 1);
    task<> waiting = waits_on(semaphore);

    semaphore.release();
    release_once_more_there(semaphore);
}

// clang-format off: the test reads the line of the call as its own line + 1
constexpr std::uint_least32_t line_of_the_foreign_acquire = std::source_location::current().line() + 1;
void acquire_there(async_semaphore& semaphore) { (void)semaphore.acquire(cancellation_token{}); }
// clang-format on

void acquire_from_another_thread() {
    report_failures_to_stderr();

    async_semaphore semaphore(1);
    std::thread([&semaphore] { acquire_there(semaphore); }).join();
}

class AsyncSemaphoreDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!detail::checked) GTEST_SKIP() << "built without coroutine checks";
    }
};

/// A strict build's report names the line of the caller.
class AsyncSemaphoreStrictDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!detail::strict) GTEST_SKIP() << "built without STRICT_CORO";
    }
};

}  // namespace

TEST_F(AsyncSemaphoreDeathTest, APermitIsNotReleasedOverTheMaximum) {
    EXPECT_DEATH(release_over_max(), "async_semaphore: released over its maximum count");
}

TEST_F(AsyncSemaphoreStrictDeathTest, TheReportNamesTheLineOfTheReleaseOverTheMaximum) {
    EXPECT_DEATH(release_over_max(),
                 std::format("async_semaphore_tests\\.cpp\\({}\\): async_semaphore: released over",
                             line_of_the_release_over_max));
}

TEST_F(AsyncSemaphoreDeathTest, APermitHandedOverCountsAgainstTheMaximum) {
    EXPECT_DEATH(release_over_a_handed_permit(), "async_semaphore: released over its maximum count");
}

TEST_F(AsyncSemaphoreStrictDeathTest, TheReportNamesTheLineOfTheReleaseOverAHandedPermit) {
    EXPECT_DEATH(release_over_a_handed_permit(),
                 std::format("async_semaphore_tests\\.cpp\\({}\\): async_semaphore: released over",
                             line_of_the_release_over_a_handed_permit));
}

TEST_F(AsyncSemaphoreDeathTest, ASemaphoreIsNotDestroyedWhileACoroutineWaitsOnIt) {
    EXPECT_DEATH(destroy_with_a_waiter(), "async_semaphore: destroyed while a coroutine waits on it");
}

TEST_F(AsyncSemaphoreDeathTest, APermitIsAcquiredOnItsCoroutinesThread) {
    EXPECT_DEATH(acquire_from_another_thread(),
                 "async_semaphore: acquired on a thread other than its coroutines'");
}

TEST_F(AsyncSemaphoreStrictDeathTest, TheReportNamesTheLineOfTheForeignAcquire) {
    EXPECT_DEATH(acquire_from_another_thread(),
                 std::format("async_semaphore_tests\\.cpp\\({}\\): async_semaphore: acquired on a thread",
                             line_of_the_foreign_acquire));
}

// The wait is made in place and held by address: neither copied nor moved.
static_assert(!std::is_copy_constructible_v<async_semaphore::acquire_awaiter> &&
              !std::is_move_constructible_v<async_semaphore::acquire_awaiter>);
static_assert(!std::is_copy_constructible_v<async_semaphore> &&
              !std::is_move_constructible_v<async_semaphore>);
