// The checks of an awaitable: awaited after a move, or awaited twice. Each ends the
// process -- assert in a Debug build, core::abort under STRICT_CORO in any build; a build
// with neither has nothing to stop at, and the misuse is undefined behaviour there.
#include <gtest/gtest.h>

#include <crtdbg.h>

#include "sta_pool.h"

import std;
import wxl.core;
import wxl.async;

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

class answers : public async_op_t<int>
{
protected:
    bool execute() override {
        set_value(42);
        return true;
    }
};

awaitable<int> ask_the_worker() {
    return sta_loop::async_run(std::unique_ptr<async_op_t<int>>(new answers));
}

task<int> awaits_twice(awaitable<int>& op) {
    (void)co_await op;
    co_return co_await op;
}

// The co_await a strict build names sits alone on the line below the one that records it.
constexpr std::uint_least32_t line_of_the_await = std::source_location::current().line() + 1;
task<int> awaits(awaitable<int>& op) { co_return co_await op; }

void await_a_moved_from_one() {
    report_failures_to_stderr();

    awaitable<int> op = ask_the_worker();
    awaitable<int> moved = std::move(op);
    task<int> t = awaits(op);
}

void await_one_twice() {
    report_failures_to_stderr();

    awaitable<int> op = ask_the_worker();
    task<int> t = awaits_twice(op);
    sta_loop::run_until([&t] { return t.done(); });
}

class AwaitableDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!coro_detail::checked) GTEST_SKIP() << "built without coroutine checks";
        GTEST_FLAG_SET(death_test_style, "threadsafe");
    }
};

class AwaitableStrictDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!coro_detail::strict) GTEST_SKIP() << "built without STRICT_CORO";
        GTEST_FLAG_SET(death_test_style, "threadsafe");
    }
};

}  // namespace

TEST_F(AwaitableDeathTest, AMovedFromOneIsNotAwaited) {
    EXPECT_DEATH(await_a_moved_from_one(), "awaitable: moved-from");
}

// The value went with the first co_await; the second finds none.
TEST_F(AwaitableDeathTest, OneIsAwaitedOnce) {
    EXPECT_DEATH(await_one_twice(), "async_op: finished without a result");
}

TEST_F(AwaitableStrictDeathTest, AMovedFromOneNamesTheCoAwait) {
    EXPECT_DEATH(await_a_moved_from_one(),
                 std::format("awaitable_tests\\.cpp\\({}\\): awaitable: moved-from", line_of_the_await));
}
