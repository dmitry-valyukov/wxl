// A mutex the coroutines of one thread lock without blocking it, on either loop: built into both
// binaries, as the semaphore suite it stands on is. A coroutine that finds it held waits, and the
// unlock hands the lock over without resuming anybody: the holder's own code after its unlock runs
// first, and the next holder on a later turn. The guard unlocks as it goes, once, and moves; a
// lock under a token ends by it and leaves the mutex to the others.
//
// Each test starts with nothing out and leaves nothing out.
#include <crtdbg.h>

#include <gtest/gtest.h>

#include "loop_environment.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

/// Locks the mutex, holds it until it is signalled, and unlocks it -- writing down each step,
/// the one after the unlock among them.
task<> holds_until_signalled(async_mutex& mutex, async_semaphore& signal,
                             std::vector<std::string>& log) {
    async_mutex::guard held = co_await mutex.lock(cancellation_token{});
    log.push_back("first locked");

    co_await signal.acquire(cancellation_token{});

    held.unlock();
    log.push_back("first after its unlock");
}

/// Locks the mutex, writes down that it got there, and unlocks it as it ends.
task<> locks(async_mutex& mutex, std::vector<std::string>& log, std::string name,
             cancellation_token token) {
    const async_mutex::guard held = co_await mutex.lock(std::move(token));
    log.push_back(name + " locked");
}

class AsyncMutexTest : public ::testing::Test
{
protected:
    void SetUp() override { ASSERT_EQ(sta_loop::outstanding(), 0u); }

    void TearDown() override { EXPECT_EQ(sta_loop::outstanding(), 0u); }
};

}  // namespace

// The second coroutine waits while the first holds the lock. The first's unlock hands it over
// without resuming the second there: the first runs on to its end, and the second is resumed
// on a later turn.
TEST_F(AsyncMutexTest, TheNextHolderIsResumedAfterTheUnlockingCodeHasRunOn) {
    async_mutex mutex;
    async_semaphore signal(0);
    std::vector<std::string> log;

    task<> first = holds_until_signalled(mutex, signal, log);
    task<> second = locks(mutex, log, "second", cancellation_token{});

    EXPECT_TRUE(!first.done() && !second.done());
    EXPECT_EQ(log, std::vector<std::string>{"first locked"});

    signal.release();
    sta_loop::run_until(first);
    sta_loop::run_until(second);

    // Resumed inside the unlock, the second would have written its line before the first's last.
    // Which turn of the loop resumes it is the loop's own: the sleeping one stops once the first
    // is done, a drain under the dispatcher queue takes every turn in line.
    EXPECT_EQ(log, (std::vector<std::string>{"first locked", "first after its unlock",
                                             "second locked"}));
    EXPECT_TRUE(mutex.try_lock().owns_lock()) << "the guard the co_await gave did not unlock as it went";
}

// The guard unlocks as it goes; one unlocked early does nothing more as it goes; try_lock() owns
// the lock of a free mutex and nothing of a held one.
TEST_F(AsyncMutexTest, TheGuardUnlocksOnce) {
    async_mutex mutex;

    {
        const async_mutex::guard held = mutex.try_lock();
        ASSERT_TRUE(held.owns_lock());
        EXPECT_TRUE(held);

        const async_mutex::guard refused = mutex.try_lock();
        EXPECT_FALSE(refused.owns_lock()) << "a held mutex was locked again";
        EXPECT_FALSE(refused);
    }

    {
        async_mutex::guard held = mutex.try_lock();
        ASSERT_TRUE(held.owns_lock());

        held.unlock();
        EXPECT_FALSE(held.owns_lock());

        // Locked and unlocked again while the guard unlocked early is still alive: the guard's
        // destructor must not unlock it a second time, which a build that checks would stop at.
        EXPECT_TRUE(mutex.try_lock().owns_lock());
    }

    // A destructor that unlocked a second time would leave two permits, and a build without the
    // checks would not stop at it: the second lock would then be let in beside the first.
    const async_mutex::guard held = mutex.try_lock();
    ASSERT_TRUE(held.owns_lock());
    EXPECT_FALSE(mutex.try_lock().owns_lock()) << "a guard unlocked early unlocked again as it went";
}

// The lock moves with the guard; the guard assigned over unlocks what it held.
TEST_F(AsyncMutexTest, TheLockMovesWithTheGuard) {
    async_mutex mutex;
    async_mutex other;

    {
        async_mutex::guard held = mutex.try_lock();
        async_mutex::guard moved(std::move(held));

        EXPECT_FALSE(held.owns_lock());
        EXPECT_TRUE(moved.owns_lock());
        EXPECT_FALSE(mutex.try_lock().owns_lock());
    }

    EXPECT_TRUE(mutex.try_lock().owns_lock());

    {
        async_mutex::guard held = mutex.try_lock();
        async_mutex::guard replaced = other.try_lock();

        replaced = std::move(held);

        EXPECT_TRUE(other.try_lock().owns_lock()) << "the guard assigned over kept its lock";
        EXPECT_FALSE(mutex.try_lock().owns_lock()) << "the lock did not move";
        EXPECT_FALSE(held.owns_lock());
    }

    EXPECT_TRUE(mutex.try_lock().owns_lock());
}

// A lock cancelled while it waits ends with the cancellation and takes nothing: the mutex stays
// its holder's, and goes to the next in line when it is unlocked.
TEST_F(AsyncMutexTest, ALockCancelledWhileItWaitsLeavesTheMutexToTheOthers) {
    async_mutex mutex;
    cancellation_source source;
    std::vector<std::string> log;

    async_mutex::guard held = mutex.try_lock();
    ASSERT_TRUE(held.owns_lock());

    task<> canceled = locks(mutex, log, "cancelled", source.token());
    task<> patient = locks(mutex, log, "patient", cancellation_token{});

    source.cancel();
    EXPECT_THROW(sta_loop::run_until(canceled), operation_canceled_exception);
    EXPECT_FALSE(patient.done());
    EXPECT_FALSE(mutex.try_lock().owns_lock());

    held.unlock();
    sta_loop::run_until(patient);

    EXPECT_EQ(log, std::vector<std::string>{"patient locked"});
    EXPECT_TRUE(mutex.try_lock().owns_lock());
}

// A guard unlocked twice stops the process in a build that checks coroutines: a lock given back
// twice would let two holders in. So does a mutex destroyed while it is locked: the guard that
// holds it would unlock into a mutex that is gone. Assert in a Debug build, core::abort under
// STRICT_CORO -- naming the line of the destructor, which no caller gives.

namespace {

/// A failed check goes to stderr and ends the process without a dialog, so
/// that a death test can read why.
void report_failures_to_stderr() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

void unlock_twice() {
    report_failures_to_stderr();

    async_mutex mutex;
    async_mutex::guard held = mutex.try_lock();
    held.unlock();
    held.unlock();
}

void destroy_while_locked() {
    report_failures_to_stderr();

    auto mutex = std::make_unique<async_mutex>();
    const async_mutex::guard held = mutex->try_lock();
    mutex.reset();
}

class AsyncMutexDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!detail::checked) GTEST_SKIP() << "built without coroutine checks";
    }
};

/// A strict build's report names the line of the check.
class AsyncMutexStrictDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!detail::strict) GTEST_SKIP() << "built without STRICT_CORO";
    }
};

}  // namespace

TEST_F(AsyncMutexDeathTest, AGuardIsNotUnlockedTwice) {
    EXPECT_DEATH(unlock_twice(), "async_mutex::guard: unlocked without owning the lock");
}

TEST_F(AsyncMutexDeathTest, AMutexIsNotDestroyedWhileLocked) {
    EXPECT_DEATH(destroy_while_locked(), "async_mutex: destroyed while locked");
}

TEST_F(AsyncMutexStrictDeathTest, TheReportOfAMutexDestroyedWhileLockedNamesItsDestructor) {
    EXPECT_DEATH(destroy_while_locked(),
                 "async_mutex\\.ixx\\(\\d+\\): async_mutex: destroyed while locked");
}

// A guard moves and is not copied; the wait is made in place.
static_assert(std::is_nothrow_move_constructible_v<async_mutex::guard> &&
              std::is_nothrow_move_assignable_v<async_mutex::guard>);
static_assert(!std::is_copy_constructible_v<async_mutex::guard> &&
              !std::is_copy_assignable_v<async_mutex::guard>);
static_assert(!std::is_move_constructible_v<async_mutex::lock_awaiter>);

// Nothing of its own beyond the semaphore, and the guard is one pointer.
static_assert(sizeof(async_mutex) == sizeof(async_semaphore));
static_assert(sizeof(async_mutex::guard) == sizeof(void*));
