// The task of an operation, which a coroutine awaits the same way as the task of a
// coroutine, and the checks the two share: awaited after a move, its value taken twice,
// joined by a second coroutine while the first waits, destroyed while a coroutine joins it,
// awaited from another thread. Each ends the process -- assert in a Debug build,
// core::abort under STRICT_CORO in any build; a build with neither has nothing to stop at,
// and the misuse is undefined behaviour there. And what a join of a kept operation does
// when its waiter goes first.
#include <gtest/gtest.h>

#include <crtdbg.h>

#include "sta_pool.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

/// A failed check goes to stderr and ends the process without a dialog, so that a death
/// test can read why. The death test runs in a process of its own, started afresh, which
/// has the loop's worker: a forked copy of this one would not.
void report_failures_to_stderr() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

/// Answers 42 on the worker -- once the gate is open, if it is given one.
class answers : public async_op_t<int>
{
public:
    explicit answers(hevent* gate) noexcept : gate_(gate) {}

protected:
    bool execute() override {
        if (gate_) gate_->wait();

        set_value(42);
        return true;
    }

private:
    hevent* gate_;
};

task<int> ask_the_worker(hevent* gate = nullptr) {
    return sta_loop::async_run(std::unique_ptr<async_op_t<int>>(new answers(gate)));
}

/// Takes the coroutine to a thread of its own and resumes it there, waiting for it to
/// suspend again or end: an awaitable of somebody else's that moves a body off its thread,
/// as a hop to a thread pool would. Only a coroutine returning a task awaits a task, so
/// this is how one comes to await it from another thread.
struct to_another_thread {
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> here) const { std::thread([here] { here.resume(); }).join(); }
    void await_resume() const noexcept {}
};

/// Joins an operation somebody else keeps: co_await of an lvalue.
task<> joins(task<int>& kept, int& got) {
    got = co_await kept;
}

task<int> awaits_twice(task<int>& op) {
    (void)co_await op;
    co_return co_await op;
}

// Each misuse a strict build reports with the line of the co_await sits alone on the line
// below the one that records it.

// clang-format off: the test reads the line of the call as its own line + 1
constexpr std::uint_least32_t line_of_the_await = std::source_location::current().line() + 1;
task<int> awaits(task<int>& op) { co_return co_await op; }
// clang-format on

// clang-format off: the test reads the line of the call as its own line + 1
constexpr std::uint_least32_t line_of_the_second_join = std::source_location::current().line() + 1;
task<> joins_second(task<int>& kept) { (void)co_await kept; }
// clang-format on

// clang-format off: the test reads the line of the call as its own line + 1
constexpr std::uint_least32_t line_of_the_foreign_join = std::source_location::current().line() + 1;
task<> joins_there(task<int>& kept) { co_await to_another_thread{}; (void)co_await kept; }
// clang-format on

// clang-format off: the test reads the line of the call as its own line + 1
constexpr std::uint_least32_t line_of_the_second_take = std::source_location::current().line() + 1;
task<int> takes_twice(task<int>& op) { (void)co_await op; co_return co_await op; }
// clang-format on

// clang-format off: the test reads the line of the call as its own line + 1
constexpr std::uint_least32_t line_of_the_early_ask = std::source_location::current().line() + 1;
void ask(task<int>& op) { (void)op.result(); }
// clang-format on

void await_a_moved_from_one() {
    report_failures_to_stderr();

    task<int> op = ask_the_worker();
    task<int> moved = std::move(op);
    task<int> t = awaits(op);
}

void await_one_twice() {
    report_failures_to_stderr();

    task<int> op = ask_the_worker();
    task<int> t = awaits_twice(op);
    sta_loop::run_until([&t] { return t.done(); });
}

void take_the_value_twice() {
    report_failures_to_stderr();

    task<int> op = sta_loop::call_here([] { return 42; });
    task<int> t = takes_twice(op);
}

void ask_before_it_is_back() {
    report_failures_to_stderr();

    hevent gate{true};
    task<int> op = ask_the_worker(&gate);
    ask(op);
}

void join_while_another_waits() {
    report_failures_to_stderr();

    hevent gate{true};
    int got = 0;
    task<int> op = ask_the_worker(&gate);
    task<> one = joins(op, got);
    task<> two = joins_second(op);
}

void drop_a_joined_operation() {
    report_failures_to_stderr();

    hevent gate{true};
    int got = 0;
    task<int> op = ask_the_worker(&gate);
    task<> joiner = joins(op, got);
    op = {};
}

void await_from_another_thread() {
    report_failures_to_stderr();

    hevent gate{true};
    task<int> op = ask_the_worker(&gate);
    task<> joiner = joins_there(op);
}

/// The place a strict build's report names: the file's own name and the line, in the
/// form core::abort writes them.
std::string place_of(std::uint_least32_t line) {
    return std::format("task_operation_tests\\.cpp\\({}\\): ", line);
}

/// Death tests of the checks: skipped where the build has none. Each runs in a process
/// started afresh, which has the loop.
class TaskOperationDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!detail::checked) GTEST_SKIP() << "built without coroutine checks";
        GTEST_FLAG_SET(death_test_style, "threadsafe");
    }
};

/// The report of a strict build: the reason, and the line of the co_await.
class TaskOperationStrictDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!detail::strict) GTEST_SKIP() << "built without STRICT_CORO";
        GTEST_FLAG_SET(death_test_style, "threadsafe");
    }
};

}  // namespace

// A coroutine that joins an operation kept elsewhere goes first: the operation comes back
// and resumes nobody -- the waiter took itself off on its way out.
TEST(TaskOperationTest, AJoinThatOutlivesItsWaiterResumesNobody) {
    hevent gate{true};
    int got = 0;
    task<int> kept = ask_the_worker(&gate);
    {
        task<> joiner = joins(kept, got);
        EXPECT_FALSE(joiner.done());
    }

    gate.set();
    const int value = sta_loop::run_until(kept);

    EXPECT_EQ(got, 0);
    EXPECT_EQ(value, 42);
}

TEST(TaskOperationTest, TheNextWaiterJoinsOnceTheFirstHasLeft) {
    hevent gate{true};
    int first = 0, second = 0;
    task<int> kept = ask_the_worker(&gate);
    {
        task<> leaves = joins(kept, first);
    }
    task<> stays = joins(kept, second);

    gate.set();
    sta_loop::run_until(stays);

    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, 42);
}

// A field that holds a task at one time and not at another: taken out with std::exchange, the
// task leaves the field empty and goes on where it was taken to, out on the worker until the
// loop brings it back.
TEST(TaskOperationTest, ATaskTakenOutOfAFieldLeavesItEmptyAndGoesOn) {
    hevent gate{true};
    task<int> field = ask_the_worker(&gate);

    task<int> taken = std::exchange(field, {});

    EXPECT_FALSE(field);
    EXPECT_TRUE(taken);
    EXPECT_FALSE(taken.done());

    gate.set();
    EXPECT_EQ(sta_loop::run_until(taken), 42);
}

// The value goes once; a failure stays, and is thrown each time it is asked, as a
// coroutine's is.
TEST(TaskOperationTest, AFailureIsThrownEachTimeItIsAsked) {
    task<> failed = sta_loop::async_call([] { throw std::runtime_error("on the worker"); });

    EXPECT_THROW(sta_loop::run_until(failed), std::runtime_error);
    EXPECT_THROW(failed.result(), std::runtime_error);
}

TEST_F(TaskOperationDeathTest, AMovedFromOneIsNotAwaited) {
    EXPECT_DEATH(await_a_moved_from_one(), "task: empty -- default-constructed or moved-from");
}

// The value went with the first co_await; the second finds it taken.
TEST_F(TaskOperationDeathTest, TheValueIsTakenOnce) {
    EXPECT_DEATH(await_one_twice(), "already been taken");
}

TEST_F(TaskOperationDeathTest, TheResultIsAskedOnceItIsBack) {
    EXPECT_DEATH(ask_before_it_is_back(), "before it ended");
}

TEST_F(TaskOperationDeathTest, ASecondCoroutineMayNotJoinWhileTheFirstWaits) {
    EXPECT_DEATH(join_while_another_waits(), "second coroutine");
}

TEST_F(TaskOperationDeathTest, AJoinedOperationOutlivesTheWait) {
    EXPECT_DEATH(drop_a_joined_operation(), "while a coroutine joins it");
}

TEST_F(TaskOperationDeathTest, AnOperationIsAwaitedOnItsOwnThread) {
    EXPECT_DEATH(await_from_another_thread(), "other than its own");
}

TEST_F(TaskOperationStrictDeathTest, AMovedFromOneNamesTheCoAwait) {
    EXPECT_DEATH(await_a_moved_from_one(),
                 place_of(line_of_the_await) + "task: empty -- default-constructed or moved-from");
}

TEST_F(TaskOperationStrictDeathTest, TheSecondTakeNamesItsCoAwait) {
    EXPECT_DEATH(take_the_value_twice(),
                 place_of(line_of_the_second_take) + "task: the value has already been taken");
}

TEST_F(TaskOperationStrictDeathTest, AnEarlyResultNamesItsLine) {
    EXPECT_DEATH(ask_before_it_is_back(),
                 place_of(line_of_the_early_ask) + "task: result\\(\\) asked before it ended");
}

TEST_F(TaskOperationStrictDeathTest, ASecondJoinNamesItsCoAwait) {
    EXPECT_DEATH(join_while_another_waits(),
                 place_of(line_of_the_second_join) +
                     "task: awaited by a second coroutine while the first one still waits");
}

TEST_F(TaskOperationStrictDeathTest, AForeignJoinNamesItsCoAwait) {
    EXPECT_DEATH(await_from_another_thread(),
                 place_of(line_of_the_foreign_join) + "task: awaited from a thread other than its own");
}

// What the loop starts and what a coroutine returns are one type, one pointer wide.
static_assert(std::is_same_v<decltype(sta_loop::async_call(std::declval<int (*)()>())), task<int>>);
static_assert(std::is_same_v<decltype(sta_loop::async_call(orphanable, std::declval<int (*)()>())), task<int>>);
static_assert(std::is_same_v<decltype(sta_loop::call_here(std::declval<int (*)()>())), task<int>>);
static_assert(sizeof(task<int>) == sizeof(void*));
