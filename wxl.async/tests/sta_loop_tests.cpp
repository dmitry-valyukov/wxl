#include <gtest/gtest.h>

#include <objbase.h>   // CoGetApartmentType / APTTYPE -- проверить, что рабочий поток MTA

#include "sta_pool.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

task add_up(int count, int& sum) {
    for (int i = 1; i <= count; ++i) sum += co_await sta_loop::async_call([i] { return i; });
}

task throws_from_the_worker(std::string& message) {
    try {
        co_await sta_loop::async_call([] { throw std::runtime_error("from the worker"); });
        message = "no exception";
    } catch (const std::exception& e) {
        message = e.what();
    }
}

/// Reads back, from the worker itself, which COM apartment it stands in.
task worker_apartment(APTTYPE& out) {
    out = co_await sta_loop::async_call([] {
        APTTYPE type = APTTYPE_CURRENT;
        APTTYPEQUALIFIER qualifier = APTTYPEQUALIFIER_NONE;
        ::CoGetApartmentType(&type, &qualifier);
        return type;
    });
}

/// An operation written out as a class rather than handed in as a lambda --
/// which is what `async_run` is for. This one only adds two numbers; what a real
/// one is after is what a lambda cannot hold: state that survives being executed
/// more than once, or a body that answers `false` and belongs, until it fires,
/// to whatever it is waiting for.
class sum_op : public async_op_t<int>
{
public:
    sum_op(int left, int right, std::thread::id& ran_on)
        : left_(left), right_(right), ran_on_(ran_on) {}

protected:
    bool execute() override {
        ran_on_ = std::this_thread::get_id();

        set_value(left_ + right_);

        return true;
    }

private:
    int left_;
    int right_;

    /// Where the answer goes besides the result: the op itself dies with the
    /// co_await that awaited it, so anything the test wants to see afterwards
    /// has to live outside it.
    std::thread::id& ran_on_;
};

task run_sum_op(int& out, std::thread::id& ran_on) {
    // Spelled as the base: `async_run` deduces R from the pointer it is given,
    // and deduction does not see through a derived class.
    std::unique_ptr<async_op_t<int>> op = std::make_unique<sum_op>(20, 22, ran_on);

    out = co_await sta_loop::async_run(std::move(op));
}

/// A stand-in for a thread with a message loop: the worker leaves a note here,
/// and the "UI thread" -- the test itself -- comes back and drains the loop.
///
/// A real application posts to its dispatcher instead; what matters for the test
/// is that the callback arrives on the worker's thread and that nothing sleeps
/// on the STA side.
class fake_dispatcher
{
public:
    void poke() noexcept {
        poked_on_ = std::this_thread::get_id();
        pokes_.fetch_add(1, std::memory_order_relaxed);
        posted_.set();
    }

    /// Waits for the next poke, the way a message loop waits for a message --
    /// no spinning, no yielding, no looking again in a moment. An auto-reset
    /// event, so a burst of pokes is one piece of work, which is exactly what a
    /// dispatcher does with them.
    void wait_for_a_poke() { posted_.wait(); }

    int pokes() const noexcept { return pokes_.load(std::memory_order_relaxed); }

    std::thread::id poked_on() const noexcept { return poked_on_; }

private:
    std::atomic<int> pokes_{0};
    hevent posted_{false};
    std::thread::id poked_on_;
};

}  // namespace

TEST(StaLoopTest, RunsACoroutineToItsEnd) {
    int sum = 0;
    task work = add_up(10, sum);

    sta_loop::run_until([&] { return work.done(); });
    work.result();

    EXPECT_EQ(sum, 55);
}

TEST(StaLoopTest, CarriesAnExceptionBackToTheCoAwait) {
    std::string message;
    task work = throws_from_the_worker(message);

    sta_loop::run_until([&] { return work.done(); });
    work.result();

    EXPECT_EQ(message, "from the worker");
}

TEST(StaLoopTest, TheWorkerThreadIsAComMtaApartment) {
    // A worker body may call winrt -- Win2D decoding an image off the UI thread,
    // say -- and winrt needs COM up on the thread. thread_group brings every
    // worker up as MTA; this reads that back from the worker itself.
    APTTYPE type = APTTYPE_CURRENT;
    task work = worker_apartment(type);

    sta_loop::run_until([&] { return work.done(); });
    work.result();

    EXPECT_EQ(type, APTTYPE_MTA);
}

TEST(StaLoopTest, RunPendingResumesWhatHasComeBackAndDoesNotWait) {
    // Nothing has been asked for, so there is nothing to resume -- and, above
    // all, this returns instead of sleeping.
    EXPECT_EQ(sta_loop::run_pending(), 0u);
}

TEST(StaLoopTest, RunPendingTakesWhateverHasPiledUp) {
    // Three coroutines started before anything is drained, so their operations
    // pile up in the return channel. run_one() sleeps until the first of them is
    // back; run_pending() then takes everything else that has arrived by now,
    // without waiting for what has not.
    int first = 0, second = 0, third = 0;

    task a = add_up(1, first);
    task b = add_up(1, second);
    task c = add_up(1, third);

    while (!(a.done() && b.done() && c.done())) {
        sta_loop::run_one();
        sta_loop::run_pending();
    }

    a.result();
    b.result();
    c.result();

    EXPECT_EQ(first, 1);
    EXPECT_EQ(second, 1);
    EXPECT_EQ(third, 1);
}

TEST(StaLoopTest, RunsAnOperationWrittenAsAClass) {
    int sum = 0;
    std::thread::id ran_on;

    task work = run_sum_op(sum, ran_on);

    sta_loop::run_until([&] { return work.done(); });
    work.result();

    EXPECT_EQ(sum, 42);
    EXPECT_NE(ran_on, std::thread::id{});
    EXPECT_NE(ran_on, std::this_thread::get_id()) << "the body belongs on the worker thread";
}

/// The driven shape, tested where it lives.
///
/// The loop of a test binary is started once and sleeps, so the shape a WinUI
/// application uses -- `sta_loop::start_driven`, a callback into a dispatcher --
/// cannot be stood up beside it. What carries the whole of the difference is
/// `sta_signal`, and that is what this asks: given a callback, a handover calls
/// it and never touches the event; given none, it releases the sleeper.
TEST(StaSignalTest, ADrivenSignalCallsBackInsteadOfWakingASleeper) {
    fake_dispatcher dispatcher;
    sta_signal signal;

    EXPECT_FALSE(signal.driven());

    signal.wake_with([&dispatcher]() noexcept { dispatcher.poke(); });

    EXPECT_TRUE(signal.driven());

    // On this thread, because there is no worker in this test -- what the
    // handover proves is that set() goes to the callback and not to the event,
    // which is why the wait() below would never return if it did not.
    signal.set();

    EXPECT_EQ(dispatcher.pokes(), 1);
    EXPECT_EQ(dispatcher.poked_on(), std::this_thread::get_id());
}

TEST(StaSignalTest, ASleepingSignalIsReleasedByTheHandover) {
    sta_signal signal;

    signal.set();

    // Already signalled, so this returns rather than sleeping -- the auto-reset
    // event is what makes a wakeup mean "look again".
    signal.wait();
}

TEST(StaSignalTest, AHeldSignalWakesTheWaiterAndStillCallsBack) {
    // Held for a wait in place, a driven signal sets the event for the waiter -- and calls
    // back regardless: the handover may have taken a trigger armed before the hold, by a
    // drain that has returned to its dispatcher and is owed this callback.
    fake_dispatcher dispatcher;
    sta_signal signal;

    signal.wake_with([&dispatcher]() noexcept { dispatcher.poke(); });
    signal.hold();
    signal.set();
    signal.release();

    EXPECT_EQ(dispatcher.pokes(), 1) << "a held signal swallowed the callback";

    // Already set, so this returns rather than sleeping.
    signal.wait_held();
}
