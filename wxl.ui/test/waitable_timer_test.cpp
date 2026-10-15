// A wait on a timer with three ends that are one to the coroutine -- the tick, the owner's
// signal(), the token's cancellation -- and a latch: a tick or a signal() that finds nobody
// waiting is kept for the next wait, and a restart() forgets it.
//
// No window, no dispatcher, no runtime. The timer is a fake with the four calls the wait
// subscribes and drives through, and the test ticks it. The queue is the one event_cancel_test
// stands for: the turn a cancelled wait asks for is impl::resume_canceled_waits(), called when
// the test says, and the definition of impl::post_canceled_waits() is that test's, shared in
// this binary.
#include <gtest/gtest.h>

#include <functional>
#include <string>
#include <vector>

#include "impl/waitable_timer.h"

namespace {

using wxl::EventToken;
using wxl::async::cancellation_source;
using wxl::async::cancellation_token;
using wxl::async::operation_canceled_exception;
using wxl::async::task;

/// What the fake timer keeps: the handler the wait subscribed, and what was asked of it.
struct timer_state {
    std::function<void()> tick;
    bool running = false;
    int started = 0;
    int stopped = 0;
    int subscribed = 0;
    int unsubscribed = 0;
};

/// The timer a wait is made over: the four calls, as impl::dispatcher_timer has them.
struct fake_timer {
    timer_state* state;

    template <typename Handler>
    EventToken add(Handler&& handler) const {
        state->tick = std::forward<Handler>(handler);
        ++state->subscribed;
        return EventToken{1};
    }

    void remove(EventToken) const {
        state->tick = {};
        ++state->unsubscribed;
    }

    void start() const {
        state->running = true;
        ++state->started;
    }

    void stop() const {
        state->running = false;
        ++state->stopped;
    }
};

using timer_wait = wxl::impl::waitable_timer<fake_timer>;

/// The interval is up, as the queue would say it. The handler is held across the call, as
/// the runtime holds its delegate: the coroutine it resumes may end before the call returns.
void tick(timer_state& state) {
    std::function<void()> const handler = state.tick;
    handler();
}

/// Waits on the timer over and over, counting how often it was let through, until the token
/// says to stop -- read after the wait, as a loop that writes between waits reads it.
task<> keeps_waiting(timer_wait& timer, cancellation_token stop, int& woken) {
    do {
        co_await timer;
        ++woken;
    } while (!stop.is_canceled());
}

/// Between two waits on the timer, waits on something else -- so that whatever the timer
/// does in between finds nobody waiting on it.
task<> waits_elsewhere_in_between(timer_wait& timer, timer_wait& gate,
                                  std::vector<std::string>& log) {
    co_await timer;
    log.push_back("first");

    co_await gate;
    log.push_back("gate");

    co_await timer;
    log.push_back("second");
}

/// Waits on the gate first, then on the timer: what the timer did before has to count, or not.
task<> waits_after_the_gate(timer_wait& gate, timer_wait& timer, std::vector<std::string>& log) {
    co_await gate;
    log.push_back("gate");

    co_await timer;
    log.push_back("timer");
}

/// One wait, and the way it ended.
task<> waits_once(timer_wait& timer, std::string& ended) {
    try {
        co_await timer;
        ended = "through";
    } catch (operation_canceled_exception const&) {
        ended = "cancelled";
    }
}

/// Every test begins with no wait in either list and no turn in the queue, and leaves it so.
class WaitableTimerTest : public ::testing::Test
{
protected:
    void SetUp() override {
        ASSERT_TRUE(wxl::impl::event_waits_drained());
        ASSERT_FALSE(wxl::impl::canceled_waits_posted());
    }

    void TearDown() override {
        EXPECT_TRUE(wxl::impl::event_waits_drained()) << "a wait was left in a list";
        EXPECT_FALSE(wxl::impl::canceled_waits_posted()) << "a turn was left in the queue";

        wxl::impl::events_closing() = false;
        wxl::impl::canceled_waits_posted() = false;
    }
};

}  // namespace

// restart() starts the timer; the tick stops it and lets the wait through, inside the tick
// handler; the coroutine is waiting again when the handler returns.
TEST_F(WaitableTimerTest, ATickEndsTheWaitAndStopsTheTimer) {
    timer_state state;
    timer_wait timer{fake_timer{&state}};
    int woken = 0;

    task<> work = keeps_waiting(timer, cancellation_token{}, woken);

    EXPECT_EQ(state.subscribed, 1);
    EXPECT_EQ(woken, 0);
    EXPECT_FALSE(state.running);

    timer.restart();
    EXPECT_TRUE(state.running);
    EXPECT_EQ(state.started, 1);

    tick(state);
    EXPECT_EQ(woken, 1);
    EXPECT_FALSE(state.running);
    EXPECT_FALSE(work.done());

    timer.restart();
    tick(state);
    EXPECT_EQ(woken, 2);
}

// signal() is the owner saying now: the wait ends inside the call, and the timer is stopped,
// so that the interval under way gives no tick of its own later.
TEST_F(WaitableTimerTest, ASignalEndsTheWaitInsideTheCallAndStopsTheTimer) {
    timer_state state;
    timer_wait timer{fake_timer{&state}};
    int woken = 0;

    task<> work = keeps_waiting(timer, cancellation_token{}, woken);

    timer.restart();
    EXPECT_TRUE(state.running);

    timer.signal();
    EXPECT_EQ(woken, 1);
    EXPECT_FALSE(state.running);
    EXPECT_FALSE(work.done());
}

// A tick that finds nobody waiting is kept: the next wait does not suspend.
TEST_F(WaitableTimerTest, ATickWhileNobodyWaitsIsKeptForTheNextWait) {
    timer_state state, gate_state;
    timer_wait timer{fake_timer{&state}};
    timer_wait gate{fake_timer{&gate_state}};
    std::vector<std::string> log;

    task<> work = waits_elsewhere_in_between(timer, gate, log);

    tick(state);
    EXPECT_EQ(log, (std::vector<std::string>{"first"}));

    tick(state);
    EXPECT_EQ(log, (std::vector<std::string>{"first"}))
        << "a tick reached a coroutine waiting elsewhere";

    gate.signal();
    EXPECT_EQ(log, (std::vector<std::string>{"first", "gate", "second"}));
    EXPECT_TRUE(work.done());
}

// And so is a signal().
TEST_F(WaitableTimerTest, ASignalWhileNobodyWaitsIsKeptForTheNextWait) {
    timer_state state, gate_state;
    timer_wait timer{fake_timer{&state}};
    timer_wait gate{fake_timer{&gate_state}};
    std::vector<std::string> log;

    task<> work = waits_elsewhere_in_between(timer, gate, log);

    timer.signal();
    timer.signal();
    EXPECT_EQ(log, (std::vector<std::string>{"first"}));

    gate.signal();
    EXPECT_EQ(log, (std::vector<std::string>{"first", "gate", "second"}));
    EXPECT_TRUE(work.done());
}

// A restart() forgets a tick not yet taken: the interval begins again, and the next wait
// suspends until it is up.
TEST_F(WaitableTimerTest, ARestartForgetsATickNotYetTaken) {
    timer_state state, gate_state;
    timer_wait timer{fake_timer{&state}};
    timer_wait gate{fake_timer{&gate_state}};
    std::vector<std::string> log;

    task<> work = waits_after_the_gate(gate, timer, log);

    timer.restart();
    tick(state);
    timer.restart();
    EXPECT_TRUE(state.running);

    gate.signal();
    EXPECT_EQ(log, (std::vector<std::string>{"gate"}))
        << "a tick before the restart let the wait through";
    EXPECT_FALSE(work.done());

    tick(state);
    EXPECT_EQ(log, (std::vector<std::string>{"gate", "timer"}));
    EXPECT_TRUE(work.done());
}

// The token's cancellation ends the wait like a tick -- nothing thrown -- on the queue's turn:
// cancel() resumes nobody, the coroutine goes round once more and reads the token.
TEST_F(WaitableTimerTest, TheTokensCancellationEndsTheWaitLikeATickOnTheQueuesTurn) {
    timer_state state;
    cancellation_source stop;
    timer_wait timer{fake_timer{&state}, stop.token()};
    int woken = 0;

    task<> work = keeps_waiting(timer, stop.token(), woken);

    timer.restart();
    tick(state);
    EXPECT_EQ(woken, 1);

    stop.cancel();
    EXPECT_FALSE(work.done()) << "cancel() resumed the coroutine itself";
    EXPECT_EQ(woken, 1);
    EXPECT_TRUE(wxl::impl::canceled_waits_posted());

    wxl::impl::resume_canceled_waits();

    ASSERT_TRUE(work.done());
    EXPECT_NO_THROW(work.result());
    EXPECT_EQ(woken, 2);
}

// Under a token cancelled already the wait does not suspend, and there is nothing for the
// queue to do.
TEST_F(WaitableTimerTest, UnderATokenCancelledAlreadyAWaitEndsAtOnce) {
    timer_state state;
    cancellation_source stop;
    stop.cancel();

    timer_wait timer{fake_timer{&state}, stop.token()};
    int woken = 0;

    task<> work = keeps_waiting(timer, stop.token(), woken);

    ASSERT_TRUE(work.done());
    EXPECT_NO_THROW(work.result());
    EXPECT_EQ(woken, 1);
    EXPECT_FALSE(wxl::impl::canceled_waits_posted());
}

// A tick before the turn ends a cancelled wait itself, and the turn then finds nobody.
TEST_F(WaitableTimerTest, ATickBeforeTheTurnEndsACancelledWait) {
    timer_state state;
    cancellation_source stop;
    timer_wait timer{fake_timer{&state}, stop.token()};
    int woken = 0;

    task<> work = keeps_waiting(timer, stop.token(), woken);

    timer.restart();
    stop.cancel();
    EXPECT_FALSE(work.done());

    tick(state);
    ASSERT_TRUE(work.done());
    EXPECT_NO_THROW(work.result());
    EXPECT_EQ(woken, 1);

    wxl::impl::resume_canceled_waits();
}

// The one end that differs: the application going down ends the wait with the cancellation
// exception, as it ends every wait, so that a loop under no cancelled token cannot spin.
TEST_F(WaitableTimerTest, GoingDownEndsTheWaitWithTheCancellationException) {
    timer_state state;
    timer_wait timer{fake_timer{&state}};
    std::string ended;

    task<> work = waits_once(timer, ended);
    EXPECT_FALSE(work.done());

    wxl::impl::close_event_waits();

    ASSERT_TRUE(work.done());
    EXPECT_EQ(ended, "cancelled");

    // And a wait begun on the way down does not suspend.
    std::string later;
    task<> late = waits_once(timer, later);
    ASSERT_TRUE(late.done());
    EXPECT_EQ(later, "cancelled");
}

// A signal that stands when the application goes down is still taken -- once; the wait after
// it ends as the others do.
TEST_F(WaitableTimerTest, ASignalStandingOnTheWayDownIsTakenOnce) {
    timer_state state;
    timer_wait timer{fake_timer{&state}};

    timer.signal();
    wxl::impl::close_event_waits();

    std::string first, second;
    task<> a = waits_once(timer, first);
    task<> b = waits_once(timer, second);

    EXPECT_EQ(first, "through");
    EXPECT_EQ(second, "cancelled");
}

// A frame destroyed while it waits leaves the timer's wait and the list; a tick afterwards
// finds nobody and is kept, and the timer going away unsubscribes and stops.
TEST_F(WaitableTimerTest, AFrameDestroyedWhileWaitingLeavesTheTimer) {
    timer_state state;
    {
        timer_wait timer{fake_timer{&state}};
        int woken = 0;

        {
            task<> work = keeps_waiting(timer, cancellation_token{}, woken);
            timer.restart();
        }

        EXPECT_TRUE(wxl::impl::event_waits_drained());

        tick(state);
        EXPECT_EQ(woken, 0);
    }

    EXPECT_EQ(state.unsubscribed, 1);
    EXPECT_FALSE(state.running);
}

// The wait answers a token's request as every wait of wxl does, and the timer is as narrow as
// the wait needs: the fake, a token and a subscription token, and the latch.
static_assert(wxl::async::detail::cancellable_awaiter<timer_wait::Wait>);
