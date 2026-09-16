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

protected:
    void run() override {
        run_calls.fetch_add(1);
        saw_thread_id.store(get_thread_id() != 0);
        while (!stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
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

void wait_until_running(const counting_component & c) {
    for (int i = 0; i < 1000 && c.run_calls.load() == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

}  // namespace

TEST(ThreadedComponentTest, DetachedThreadRunsAndJoinsAfterStop) {
    counting_component c;
    const ticket start_ticket = c.start_async();
    start_ticket.get();
    const ticket stop = c.stop_ticket();
    EXPECT_TRUE(c.was_started());

    wait_until_running(c);
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

    wait_until_running(c);
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
