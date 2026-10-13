#include "process_handles.h"

#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;



using namespace wxl::core;
using namespace wxl::async;

namespace {

class counting_component : public threaded_component {
public:
    explicit counting_component(thread_group * manager = nullptr)
        : threaded_component("counter", manager)
    {}

    ~counting_component() override {
        dispose();
    }

    std::atomic<int> run_calls{0};
    std::atomic<bool> saw_thread_id{false};

    /// Returns once run() has looked at its thread and counted itself -- both, since the
    /// release that wakes this comes after both stores.
    void wait_until_running() { running_.acquire(); }

protected:
    void run() override {
        saw_thread_id.store(get_thread_id() != 0);
        run_calls.fetch_add(1);
        running_.release();
        stopped_.acquire();
    }

    /// Lets run() return; a stop that comes before run() leaves the count for it to find.
    void on_stopping() override {
        threaded_component::on_stopping();
        stopped_.release();
    }

private:
    semaphore running_{0};
    semaphore stopped_{0};
};

class failing_component : public threaded_component {
public:
    failing_component()
        : threaded_component("failer")
    {}

    ~failing_component() override {
        dispose();
    }

protected:
    void run() override {
        throw std::runtime_error("boom");
    }
};

}  // namespace

TEST(ThreadedComponentTest, DetachedThreadRunsAndJoinsAfterStop) {
    counting_component c;
    const ticket start_ticket = c.start_async();
    start_ticket.get();
    const ticket stop = c.stop_ticket();
    EXPECT_TRUE(c.was_started());

    c.wait_until_running();
    EXPECT_EQ(c.run_calls.load(), 1);
    EXPECT_TRUE(c.saw_thread_id.load());

    c.stop_async().get();
    EXPECT_TRUE(c.stop_ticket().is_ready());
    EXPECT_EQ(c.get_thread_id(), 0u);
    EXPECT_TRUE(stop.is_ready());
}

TEST(ThreadedComponentTest, ManagedThreadRunsUnderExplicitManager) {
    thread_group_ptr manager = thread_group::create();
    counting_component c(manager.get());
    c.start_async().get();

    c.wait_until_running();
    EXPECT_EQ(c.run_calls.load(), 1);

    c.stop_async().get();
    EXPECT_TRUE(c.stop_ticket().is_ready());
    manager->join_all();
}

TEST(ThreadedComponentTest, ExceptionFromRunStopsComponentWithError) {
    failing_component c;
    const ticket start_ticket = c.start_async();
    start_ticket.get();
    const ticket stop = c.stop_ticket();

    ASSERT_EQ(stop.wait_for(duration::from_sec(5)), future_status::ready);
    EXPECT_TRUE(stop.has_exception());
}

TEST(ThreadedComponentTest, StartAfterStopRequestedNeverRuns) {
    counting_component c;
    c.stop_async();

    EXPECT_THROW(c.start_async(), operation_canceled_exception);
    EXPECT_EQ(c.run_calls.load(), 0);
}

// Starting and stopping a component leaves nothing open behind it, on a detached thread as in
// a group: its thread's handles are closed, and the states of its task trees go with it --
// those waited on along the way with the event each of them holds for that.
TEST(ThreadedComponentTest, StartStopCyclesLeaveNoHandleOpen) {
    constexpr int cycles = 64;

    const long gained = handles_gained(cycles, [] {
        {
            counting_component c;
            c.start_async().get();
            c.stop_async().get();
        }
        {
            thread_scope manager;
            counting_component c(manager);
            c.start_async().get();
            c.stop_async().get();
        }
    });

    EXPECT_LT(gained, cycles / 4);
}
