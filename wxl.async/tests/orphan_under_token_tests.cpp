// An orphanable call waited for under its owner's token, on either loop: built into both
// binaries, since an application with a window runs it under the dispatcher queue, where the
// body is carried out on the thread pool. The answer under a live token, nothing made under
// one cancelled already, and a body cancelled while it is out: the wait ends with the
// cancellation on the loop's next turn, without the body, and the body is left an orphan --
// its stop requested, what it makes thrown away, counted until it is back. And the form
// wxl's own operations on the disk take, which waits for the body instead.
//
// Each test starts with nothing out and leaves nothing out: the count the end of a process
// waits for is what it checks.
#include <gtest/gtest.h>

#include "loop_environment.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

/// What an application hands the pool: bytes of its own, moved into the body, and work of
/// many steps that asks its state between them.
task<std::size_t> counts_letters(std::string bytes, cancellation_token stop) {
    co_return co_await sta_loop::async_call(
        orphanable,
        [bytes = std::move(bytes)](const orphan_state& state) {
            std::size_t letters = 0;

            for (const char c : bytes) {
                if (state.stop_requested()) throw operation_canceled_exception();
                if (c != ' ') ++letters;
            }

            return letters;
        },
        std::move(stop));
}

/// A result that counts its living copies, to see one thrown away.
class counted
{
public:
    explicit counted(std::atomic<int>& alive) noexcept : alive_(&alive) { ++*alive_; }
    counted(counted&& other) noexcept : alive_(other.alive_) { ++*alive_; }
    counted& operator=(counted&&) = delete;
    ~counted() { --*alive_; }

private:
    std::atomic<int>* alive_;
};

/// Turns the loop until everything sent is back.
void until_nothing_is_out() {
    wait_until([] { return sta_loop::outstanding() == 0; });
}

}  // namespace

// Under a live token the body runs and its value is the answer, whether or not it takes its
// state.
TEST(OrphanUnderTokenTest, AnswersWhatTheBodyMadeUnderALiveToken) {
    ASSERT_EQ(sta_loop::outstanding(), 0u);

    cancellation_source stop;

    EXPECT_EQ(sta_loop::run_until(counts_letters("a page", stop.token())), 5u);

    task<int> plain = sta_loop::async_call(orphanable, [] { return 7; }, stop.token());
    EXPECT_EQ(sta_loop::run_until(plain), 7);

    EXPECT_EQ(sta_loop::outstanding(), 0u);
}

// Under a token cancelled already -- here through the application's source, of which the
// owner's is a child -- nothing is made: the body never runs, nothing goes out, and the
// co_await ends with the cancellation at once.
TEST(OrphanUnderTokenTest, UnderATokenCancelledAlreadyNothingIsMade) {
    ASSERT_EQ(sta_loop::outstanding(), 0u);

    cancellation_source application;
    cancellation_source owner(application.token());
    application.cancel();

    std::atomic<bool> ran{false};

    task<int> refused = sta_loop::async_call(
        orphanable,
        [&ran] {
            ran = true;
            return 1;
        },
        owner.token());

    EXPECT_TRUE(refused.done());
    EXPECT_THROW((void)refused.result(), operation_canceled_exception);
    EXPECT_FALSE(ran);
    EXPECT_EQ(sta_loop::outstanding(), 0u);
}

// Cancelled while the body is held at a gate -- a step nothing interrupts -- the wait is
// answered at once: one pass of the loop that waits for nothing ends it, the gate still
// shut. The body, an orphan from that moment, finds its stop requested once through the
// gate, and is counted until it is back.
TEST(OrphanUnderTokenTest, CancelledWhileItRunsTheWaitEndsWithoutTheBody) {
    ASSERT_EQ(sta_loop::outstanding(), 0u);

    hevent started{true}, gate{true};
    std::atomic<bool> saw_stop_requested{false};
    std::atomic<bool> finished{false};
    cancellation_source stop;
    const orphans_drained_on_exit drained(gate);

    task<int> held = sta_loop::async_call(
        orphanable,
        [&started, &gate, &saw_stop_requested, &finished](const orphan_state& state) {
            started.set();
            gate.wait();

            saw_stop_requested = state.stop_requested();
            finished = true;

            if (state.stop_requested()) throw operation_canceled_exception();

            return 1;
        },
        stop.token());

    started.wait();
    stop.cancel();

    // The turn the cancel gave the loop, taken without waiting for anything.
    sta_loop::run_pending();

    ASSERT_TRUE(held.done()) << "the wait waits for the body";
    EXPECT_FALSE(finished);
    EXPECT_THROW((void)held.result(), operation_canceled_exception);
    EXPECT_EQ(sta_loop::outstanding(), 1u) << "the orphan is not counted";

    gate.set();
    until_nothing_is_out();

    EXPECT_TRUE(finished);
    EXPECT_TRUE(saw_stop_requested);
}

// A body that never asks its state runs to its end, and the wait still answers the
// cancellation: what the body made is thrown away.
TEST(OrphanUnderTokenTest, ABodyThatDoesNotAskStillAnswersTheCancellation) {
    ASSERT_EQ(sta_loop::outstanding(), 0u);

    hevent started{true}, gate{true};
    std::atomic<int> alive{0};
    std::atomic<bool> finished{false};
    cancellation_source stop;
    const orphans_drained_on_exit drained(gate);

    task<counted> held = sta_loop::async_call(
        orphanable,
        [&started, &gate, &alive, &finished] {
            started.set();
            gate.wait();

            counted made(alive);
            finished = true;
            return made;
        },
        stop.token());

    started.wait();
    stop.cancel();
    sta_loop::run_pending();

    ASSERT_TRUE(held.done());
    EXPECT_FALSE(finished);
    EXPECT_THROW((void)held.result(), operation_canceled_exception);

    gate.set();
    until_nothing_is_out();

    EXPECT_TRUE(finished);
    EXPECT_EQ(alive, 0) << "what the orphan made was kept";
}

// The race of a body ending as the token fires is settled on the STA thread. One that has
// made its value but has not been taken back by the loop answers the cancellation, and the
// value is thrown away; one taken back first keeps its value against a later request.
TEST(OrphanUnderTokenTest, WhetherTheLoopHasTakenTheBodyBackSettlesTheRace) {
    ASSERT_EQ(sta_loop::outstanding(), 0u);

    std::atomic<int> alive{0};
    hevent made_it{true};
    cancellation_source stop;
    const orphans_drained_on_exit drained;

    task<counted> late = sta_loop::async_call(
        orphanable,
        [&alive, &made_it] {
            counted made(alive);
            made_it.set();
            return made;
        },
        stop.token());

    // Waited for without turning the loop, so that the body cannot be taken back.
    made_it.wait();
    stop.cancel();
    sta_loop::run_pending();

    ASSERT_TRUE(late.done());
    EXPECT_THROW((void)late.result(), operation_canceled_exception);

    until_nothing_is_out();
    EXPECT_EQ(alive, 0) << "what the orphan made was kept";

    cancellation_source later;

    task<int> kept = sta_loop::async_call(orphanable, [] { return 3; }, later.token());
    sta_loop::run_until([&] { return kept.done(); });

    later.cancel();
    EXPECT_EQ(kept.result(), 3);
    EXPECT_EQ(sta_loop::outstanding(), 0u);
}

// A task dropped while the body is out leaves it an orphan, as the form without a token
// does; one dropped after the token fired and before the loop's turn came goes with that
// turn. Either way the body finishes alone and is counted until it is back.
TEST(OrphanUnderTokenTest, ATaskDroppedLeavesTheBodyAnOrphan) {
    for (const bool cancelled_first : {false, true}) {
        SCOPED_TRACE(cancelled_first ? "dropped after the request" : "dropped while out");

        ASSERT_EQ(sta_loop::outstanding(), 0u);

        hevent started{true}, gate{true};
        std::atomic<bool> saw_stop_requested{false};
        std::atomic<bool> finished{false};
        cancellation_source stop;
        const orphans_drained_on_exit drained(gate);

        {
            task<int> dropped = sta_loop::async_call(
                orphanable,
                [&started, &gate, &saw_stop_requested, &finished](const orphan_state& state) {
                    started.set();
                    gate.wait();

                    saw_stop_requested = state.stop_requested();
                    finished = true;

                    return 1;
                },
                stop.token());

            started.wait();

            if (cancelled_first) stop.cancel();
        }

        // The body, and for a task dropped after the request the turn the loop still owes.
        EXPECT_EQ(sta_loop::outstanding(), cancelled_first ? 2u : 1u);

        sta_loop::run_pending();
        EXPECT_EQ(sta_loop::outstanding(), 1u);
        EXPECT_FALSE(finished);

        stop.cancel();
        gate.set();
        until_nothing_is_out();

        EXPECT_TRUE(finished);
        EXPECT_TRUE(saw_stop_requested);
    }
}

// The form that waits for the body, which wxl's own operations on the disk take: cancelled while
// the body is held at a gate, its co_await does not end -- the body is told, and kept -- and
// once through the gate the body answers: the cancellation if it asks its state and stops,
// what it made if it does not. When the co_await ends, nothing of the body is out any more.
TEST(OrphanUnderTokenTest, TheFormThatWaitsForTheBodyEndsWithTheBody) {
    for (const bool asks : {true, false}) {
        SCOPED_TRACE(asks ? "the body asks its state" : "the body does not ask");

        ASSERT_EQ(sta_loop::outstanding(), 0u);

        hevent started{true}, gate{true};
        cancellation_source stop;
        const orphans_drained_on_exit drained(gate);

        task<int> held = detail::async_call_waiting_for_body(
            orphanable,
            [&started, &gate, asks](const orphan_state& state) {
                started.set();
                gate.wait();

                if (asks && state.stop_requested()) throw operation_canceled_exception();

                return 1;
            },
            stop.token());

        started.wait();
        stop.cancel();
        sta_loop::run_pending();

        EXPECT_FALSE(held.done()) << "the wait did not wait for the body";
        EXPECT_EQ(sta_loop::outstanding(), 1u);

        gate.set();
        sta_loop::run_until([&] { return held.done(); });

        EXPECT_EQ(sta_loop::outstanding(), 0u);

        if (asks) {
            EXPECT_THROW((void)held.result(), operation_canceled_exception);
        } else {
            EXPECT_EQ(held.result(), 1);
        }
    }
}
