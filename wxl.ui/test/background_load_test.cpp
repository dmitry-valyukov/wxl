// The load of a window's background on the way (impl/background_load.h): each under a source of
// its own made under its owner's token, cancelled by the next, and the detached frame that waits
// for it -- which the exit counts, which does not show what a later change has replaced, and
// whose failure is not swallowed.
//
// No window and no Win2D. The load is a task whose coroutine waits at a gate the test opens; what
// is shown is written down. That a real load stands under its token and is cut short by it is
// the operation's own, and operation_test.cpp has it.
#include <crtdbg.h>

#include <gtest/gtest.h>

#include <coroutine>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "impl/background_load.h"

namespace {

using wxl::async::cancellation_source;
using wxl::async::cancellation_token;
using wxl::async::detached_task;
using wxl::async::detached_task_failure_handler;
using wxl::async::on_detached_task_failure;
using wxl::async::task;
using wxl::impl::background_loads;
using wxl::impl::show_when_loaded;

/// Where a load waits until the test lets it on.
class gate {
public:
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> waiting) noexcept { waiting_ = waiting; }
    void await_resume() const noexcept {}

    void open() { std::exchange(waiting_, {}).resume(); }

private:
    std::coroutine_handle<> waiting_;
};

/// A picture loading: done when the gate opens.
task<std::string> loads(gate& at, std::string picture) {
    co_await at;
    co_return picture;
}

/// A picture that is not there: the load fails when the gate opens.
task<std::string> fails(gate& at) {
    co_await at;
    throw std::runtime_error("no such file");
}

using shown_t = std::vector<std::string>;

auto shows(shown_t& shown) {
    return [&shown](std::string const& picture) { shown.push_back(picture); };
}

TEST(BackgroundLoadTest, TheWaitIsAFrameTheExitCountsAndShowsWhatWasLoaded) {
    cancellation_source application;
    background_loads loads_of_window;
    gate at;
    shown_t shown;
    std::size_t const before = detached_task::live_frames();

    cancellation_token const load = loads_of_window.next(application.token());
    show_when_loaded(loads(at, "cover"), load, shows(shown));
    EXPECT_EQ(detached_task::live_frames(), before + 1) << "the call has returned, and the wait is left";
    EXPECT_TRUE(shown.empty());

    at.open();
    EXPECT_EQ(shown, shown_t{"cover"});
    EXPECT_EQ(detached_task::live_frames(), before);
}

// A background set while the picture loads keeps the last word: the load under way is cancelled,
// and its answer, when it comes anyway -- it reached the queue first -- is not shown.
TEST(BackgroundLoadTest, AnotherChangeCancelsTheLoadAndItsLateAnswerIsNotShown) {
    cancellation_source application;
    background_loads loads_of_window;
    gate at;
    shown_t shown;
    std::size_t const before = detached_task::live_frames();

    cancellation_token const load = loads_of_window.next(application.token());
    show_when_loaded(loads(at, "cover"), load, shows(shown));

    loads_of_window.cancel();
    EXPECT_TRUE(load.is_canceled());
    EXPECT_FALSE(application.is_canceled()) << "only the load's own source";

    at.open();
    EXPECT_TRUE(shown.empty());
    EXPECT_EQ(detached_task::live_frames(), before);
}

// The next load cancels the one before it, and the later picture is the one shown, whichever
// answers first.
TEST(BackgroundLoadTest, TheNextLoadCancelsTheOneBeforeIt) {
    cancellation_source application;
    background_loads loads_of_window;
    gate first_at;
    gate second_at;
    shown_t shown;
    std::size_t const before = detached_task::live_frames();

    cancellation_token const first = loads_of_window.next(application.token());
    show_when_loaded(loads(first_at, "first"), first, shows(shown));
    cancellation_token const second = loads_of_window.next(application.token());
    show_when_loaded(loads(second_at, "second"), second, shows(shown));
    EXPECT_TRUE(first.is_canceled());
    EXPECT_FALSE(second.is_canceled());

    second_at.open();
    first_at.open();
    EXPECT_EQ(shown, shown_t{"second"});
    EXPECT_EQ(detached_task::live_frames(), before);
}

// The owner's token is the parent of every load: cancelled on the way out, it cancels the one
// under way, and a load asked under it afterwards is cancelled from birth.
TEST(BackgroundLoadTest, TheOwnersTokenCancelsTheLoad) {
    cancellation_source application;
    background_loads loads_of_window;
    gate at;
    shown_t shown;

    cancellation_token const load = loads_of_window.next(application.token());
    show_when_loaded(loads(at, "cover"), load, shows(shown));

    application.cancel();
    EXPECT_TRUE(load.is_canceled());
    at.open();
    EXPECT_TRUE(shown.empty());

    EXPECT_TRUE(loads_of_window.next(application.token()).is_canceled());
}

std::exception_ptr failure;

void keep_failure(std::exception_ptr error) noexcept { failure = std::move(error); }

/// The handler of detached failures, the test's own while the fixture lives.
class BackgroundLoadFailureTest : public ::testing::Test {
protected:
    void SetUp() override {
        failure = nullptr;
        previous_ = std::exchange(on_detached_task_failure(), &keep_failure);
    }

    void TearDown() override { on_detached_task_failure() = previous_; }

private:
    detached_task_failure_handler previous_ = nullptr;
};

// A load that fails is not swallowed: it reaches the handler of detached failures, which by
// default names it and stops the process.
TEST_F(BackgroundLoadFailureTest, AFailedLoadIsNotSwallowed) {
    cancellation_source application;
    background_loads loads_of_window;
    gate at;
    shown_t shown;
    std::size_t const before = detached_task::live_frames();

    cancellation_token const load = loads_of_window.next(application.token());
    show_when_loaded(fails(at), load, shows(shown));
    at.open();

    ASSERT_TRUE(failure);
    try {
        std::rethrow_exception(failure);
    } catch (std::runtime_error const& error) {
        EXPECT_STREQ(error.what(), "no such file");
    }
    EXPECT_TRUE(shown.empty());
    EXPECT_EQ(detached_task::live_frames(), before);
}

// The cancellation that cuts the wait itself short is the frame's ordinary end, not a failure.
TEST_F(BackgroundLoadFailureTest, ACancelledWaitIsNoFailure) {
    cancellation_source application;
    background_loads loads_of_window;
    shown_t shown;
    std::size_t const before = detached_task::live_frames();

    application.cancel();
    cancellation_token const load = loads_of_window.next(application.token());
    show_when_loaded(wxl::async::detail::canceled<std::string>(), load, shows(shown));

    EXPECT_FALSE(failure);
    EXPECT_TRUE(shown.empty());
    EXPECT_EQ(detached_task::live_frames(), before);
}

}  // namespace
