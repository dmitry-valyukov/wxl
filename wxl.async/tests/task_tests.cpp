#include <gtest/gtest.h>

#include "sta_pool.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::async;

namespace {

// What the failure handler saw, since it is a bare function pointer and has
// nowhere of its own to put it.
std::exception_ptr reported;

// Puts the handler back on the way out, so one test cannot decide what the
// next one does with its exceptions.
class TaskTest : public ::testing::Test
{
protected:
    void SetUp() override {
        reported = {};
        previous_ = on_task_failure();
        on_task_failure() = [](std::exception_ptr error) noexcept { reported = error; };
    }

    void TearDown() override { on_task_failure() = previous_; }

private:
    task_failure_handler previous_ = nullptr;
};

// Something whose destruction can be seen from outside: what a coroutine
// holds has to be let go of, whichever way the coroutine ends.
struct Trace {
    bool* released;

    explicit Trace(bool* flag) noexcept : released(flag) {}
    Trace(const Trace&) = delete;
    ~Trace() { *released = true; }
};

task runs_through(int& sink) {
    sink = 1;
    co_return;
}

task waits_then_finishes(std::coroutine_handle<>& out, bool* released) {
    const Trace trace{released};

    struct capture {
        std::coroutine_handle<>* out;

        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> waiter) const noexcept { *out = waiter; }
        void await_resume() const noexcept {}
    };

    co_await capture{&out};
}

task cancelled_at_once() {
    throw operation_canceled_exception{};
    co_return;
}

task fails_at_once(std::string_view what) {
    throw std::runtime_error(std::string(what));
    co_return;
}

TEST_F(TaskTest, RunsWhereItIsCalled) {
    int sink = 0;

    runs_through(sink);

    EXPECT_EQ(sink, 1);
    EXPECT_FALSE(reported);
}

// The whole difference from task: nothing came back to hold, and the frame is
// gone by the time the body has finished. What proves it from outside is that
// what the frame held was let go of.
TEST_F(TaskTest, ReleasesItselfWhenTheBodyEnds) {
    std::coroutine_handle<> waiter;
    bool released = false;

    waits_then_finishes(waiter, &released);

    ASSERT_TRUE(waiter);
    EXPECT_FALSE(released);

    waiter.resume();

    EXPECT_TRUE(released);
    EXPECT_FALSE(reported);
}

// Cancellation is how a self-owning coroutine is told that what it waits for is
// never coming, so it is the ordinary end and not a failure.
TEST_F(TaskTest, CancellationIsNotAFailure) {
    cancelled_at_once();

    EXPECT_FALSE(reported);
}

// Anything else has nowhere to go -- there is no caller left -- so it goes to
// the handler the application named.
TEST_F(TaskTest, EverythingElseGoesToTheHandler) {
    fails_at_once("no such file");

    ASSERT_TRUE(reported);

    try {
        std::rethrow_exception(reported);
        FAIL() << "the reported exception was empty";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "no such file");
    }
}

TEST_F(TaskTest, ADestroyedFrameStillLetsGoOfWhatItHeld) {
    std::coroutine_handle<> waiter;
    bool released = false;

    waits_then_finishes(waiter, &released);
    ASSERT_TRUE(waiter);

    // The other way a suspended coroutine can end: not resumed but taken
    // apart. Its locals are destroyed all the same, which is what makes RAII
    // in a coroutine mean anything.
    waiter.destroy();

    EXPECT_TRUE(released);
}

}  // namespace
