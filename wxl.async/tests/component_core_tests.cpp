#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;



using namespace wxl::core;
using namespace wxl::async;

namespace {

class simple_component : public component {
public:
    simple_component()
        : component("simple")
    {}

    ~simple_component() override {
        dispose();
    }

    int on_starting_calls = 0;
    int on_started_calls = 0;
    int on_stopping_calls = 0;
    int on_stopped_calls = 0;

protected:
    void on_starting() override {
        component::on_starting();
        ++on_starting_calls;
        // No further async startup work -- ready immediately, matching the
        // pattern threaded_component uses once its thread is actually running.
        on_started();
    }

    void on_started() override {
        component::on_started();
        ++on_started_calls;
    }

    void on_stopping() override {
        component::on_stopping();
        ++on_stopping_calls;
    }

    void on_stopped() override {
        component::on_stopped();
        ++on_stopped_calls;
    }
};

}  // namespace

TEST(ComponentTest, StartsAndStops) {
    simple_component c;
    const ticket start_ticket = c.start_async();
    start_ticket.get();
    const ticket stop = c.stop_ticket();
    EXPECT_TRUE(c.was_started());
    EXPECT_FALSE(stop.is_ready());

    c.stop_async().get();
    EXPECT_TRUE(stop.is_ready());
    EXPECT_FALSE(stop.has_exception());
}

TEST(ComponentTest, StopWithErrorReason) {
    simple_component c;
    c.start_async().get();

    c.stop_async(stop_reason(std::string("boom"))).wait();
    ASSERT_NE(c.stop_reason_ptr(), nullptr);
}

TEST(ComponentTest, LifecycleCallbacksFireExactlyOnce) {
    simple_component c;
    bool started_cb = false;
    bool stopped_cb = false;
    c.subscribe_on_started([&]() noexcept { started_cb = true; });
    c.subscribe_on_stopped([&](const stop_reason &) noexcept { stopped_cb = true; });

    c.start_async().get();
    c.stop_async().get();

    EXPECT_TRUE(started_cb);
    EXPECT_TRUE(stopped_cb);
    EXPECT_EQ(c.on_starting_calls, 1);
    EXPECT_EQ(c.on_started_calls, 1);
    EXPECT_EQ(c.on_stopping_calls, 1);
    EXPECT_EQ(c.on_stopped_calls, 1);
}

// subscribe_on_started() builds the callback out of what the caller wrote, so a move-only
// callable goes in as it is. Through the std::function this method used to take it could
// not: that one has to copy.
TEST(ComponentTest, ASubscriberCanBeMoveOnly) {
    simple_component c;
    bool started_cb = false;
    bool stopped_cb = false;

    // The captured unique_ptr is what makes the lambda move-only; the flag it sets stays on
    // the stack, because the callbacks are dropped as soon as they have been called.
    c.subscribe_on_started([&started_cb, held = std::make_unique<int>(1)]() noexcept {
        started_cb = *held == 1;
    });
    c.subscribe_on_stopped([&stopped_cb, held = std::make_unique<int>(2)](const stop_reason&) noexcept {
        stopped_cb = *held == 2;
    });

    c.start_async().get();
    c.stop_async().get();

    EXPECT_TRUE(started_cb);
    EXPECT_TRUE(stopped_cb);
}

TEST(ComponentTest, EverySubscriberIsCalledInTheOrderItSubscribed) {
    simple_component c;
    std::vector<std::string> order;

    c.subscribe_on_started([&order]() noexcept { order.push_back("started 1"); });
    c.subscribe_on_started([&order]() noexcept { order.push_back("started 2"); });
    c.subscribe_on_stopped([&order](const stop_reason &) noexcept { order.push_back("stopped 1"); });
    c.subscribe_on_stopped([&order](const stop_reason &) noexcept { order.push_back("stopped 2"); });

    c.start_async().get();
    c.stop_async().get();

    EXPECT_EQ(order, (std::vector<std::string>{"started 1", "started 2", "stopped 1", "stopped 2"}));
}

TEST(ComponentTest, EveryStopSubscriberIsGivenTheReason) {
    simple_component c;
    std::vector<bool> due_to_error;

    c.subscribe_on_stopped([&](const stop_reason & reason) noexcept { due_to_error.push_back(reason.stopped_due_to_error()); });
    c.subscribe_on_stopped([&](const stop_reason & reason) noexcept { due_to_error.push_back(reason.stopped_due_to_error()); });

    c.start_async().get();
    c.stop_async(stop_reason(std::string("boom"))).wait();

    EXPECT_EQ(due_to_error, (std::vector<bool>{true, true}));
}

TEST(ComponentTest, ASubscriptionIsCancelledByItsCookie) {
    simple_component c;
    bool started_cb = false;
    bool stopped_cb = false;

    const cookie_t started_cookie = c.subscribe_on_started([&]() noexcept { started_cb = true; });
    const cookie_t stopped_cookie = c.subscribe_on_stopped([&](const stop_reason &) noexcept { stopped_cb = true; });

    EXPECT_TRUE(c.unsubscribe_on_started(started_cookie));
    EXPECT_FALSE(c.unsubscribe_on_started(started_cookie));  // already cancelled
    EXPECT_TRUE(c.unsubscribe_on_stopped(stopped_cookie));

    c.start_async().get();
    c.stop_async().get();

    EXPECT_FALSE(started_cb);
    EXPECT_FALSE(stopped_cb);

    // The component's own lifecycle is not what was unsubscribed from.
    EXPECT_EQ(c.on_started_calls, 1);
    EXPECT_EQ(c.on_stopped_calls, 1);
}

TEST(ComponentTest, SubscriptionsAreGoneOnceTheyHaveBeenCalled) {
    simple_component c;
    int started_calls = 0;

    const cookie_t cookie = c.subscribe_on_started([&started_calls]() noexcept { ++started_calls; });

    c.start_async().get();

    EXPECT_EQ(started_calls, 1);

    // Called once and dropped, so there is nothing left for the cookie to cancel.
    EXPECT_FALSE(c.unsubscribe_on_started(cookie));

    c.stop_async().get();

    EXPECT_EQ(started_calls, 1);
}

TEST(ComponentTest, DoubleStartIsRefusedAndChangesNothing) {
    simple_component c;
    c.start_async().get();

    EXPECT_THROW(c.start_async(), std::logic_error);

    // A refusal is not a failed start: the component the second call was refused for is
    // still the running one, untouched, and stops the ordinary way afterwards.
    EXPECT_TRUE(c.was_started());
    EXPECT_FALSE(c.stop_requested());
    EXPECT_EQ(c.on_starting_calls, 1);
    EXPECT_EQ(c.on_stopping_calls, 0);

    const ticket stop = c.stop_async();
    stop.get();
    EXPECT_FALSE(stop.has_exception());
}

TEST(ComponentTest, StartAfterStopRequestedIsRefused) {
    simple_component c;
    c.stop_async();

    EXPECT_THROW(c.start_async(), operation_canceled_exception);

    // Refused as well, and for the same reason it must change nothing: the component never
    // started, so there is nothing here to stop or roll back.
    EXPECT_FALSE(c.was_started());
    EXPECT_EQ(c.on_starting_calls, 0);
}

TEST(ComponentTest, DestructorDisposesUnstoppedComponent) {
    simple_component c;
    c.start_async().get();
    // No explicit stop(); the destructor's dispose() call should abort-stop it
    // and wait for the running task to resolve.
}

TEST(ComponentTest, ConcurrentStartStopAcrossManyComponents) {
    constexpr int n = 50;
    std::vector<std::unique_ptr<simple_component>> components;
    for (int i = 0; i < n; ++i)
        components.push_back(std::make_unique<simple_component>());

    std::vector<std::thread> threads;
    for (int i = 0; i < n; ++i) {
        threads.emplace_back([&components, i] {
            components[i]->start_async().get();
            components[i]->stop_async().get();
        });
    }
    for (auto & t : threads) t.join();

    for (auto & c : components)
        EXPECT_NE(c->stop_reason_ptr(), nullptr);
}

// Компонент, который так и не стартовал и просто разрушился, обязан разрешить
// свой тикет остановки: иначе ждущий на нём остался бы ждать вечно. Ответ здесь
// тот же, что даёт брошенный promise, -- std::future_error(broken_promise):
// никто ничего не обещал, обещавший умер.
TEST(ComponentTest, ANeverStartedComponentBreaksItsStopTicketWhenDestroyed) {
    ticket stop;

    {
        simple_component c;
        stop = c.stop_ticket();
        EXPECT_FALSE(stop.is_ready());
    }

    ASSERT_TRUE(stop.is_ready());
    ASSERT_TRUE(stop.has_exception());

    try {
        stop.get();
        FAIL() << "the ticket of a destroyed component must not report success";
    }
    catch (const std::future_error& ex) {
        EXPECT_EQ(std::future_errc::broken_promise, ex.code());
    }
}
