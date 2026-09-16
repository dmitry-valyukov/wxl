#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

/// The channel's contract, in the order a user meets it: elements arrive in the order
/// they were sent; a reader with nothing to take sleeps and is woken by the next send;
/// and closing is the writer's last word -- stored after the last element, seen by the
/// reader once it has drained everything, and reaching a reader that is asleep through
/// the one forced signal the owner sends after it.
using channel_t = spsc_channel<int, 4>;

}  // namespace

TEST(SpscChannelTest, ElementsArriveInTheOrderTheyWereSent) {
    channel_t channel{false};  // auto-reset, the shape sta_loop uses
    channel_t::reader reader(channel);

    for (int i = 0; i < 10; ++i) channel.send(i);

    for (int i = 0; i < 10; ++i) {
        int got = -1;

        ASSERT_TRUE(reader.receive(got));
        EXPECT_EQ(got, i);
    }
}

TEST(SpscChannelTest, ASleepingReaderIsWokenByTheNextSend) {
    channel_t channel{false};  // auto-reset, the shape sta_loop uses
    std::latch asleep(1);
    int got = -1;

    std::thread consumer([&] {
        channel_t::reader reader(channel);

        // Nothing to take yet, so this sleeps; the latch tells the writer it may send.
        // A receive that is not woken hangs the test, which is the failure it looks for.
        asleep.count_down();

        while (!reader.receive(got)) {
        }
    });

    asleep.wait();
    channel.send(42);
    consumer.join();

    EXPECT_EQ(got, 42);
}

TEST(SpscChannelTest, CloseAfterTheLastSendLetsTheReaderDrainEverything) {
    channel_t channel{false};  // auto-reset, the shape sta_loop uses
    int taken = 0;

    std::thread consumer([&] {
        channel_t::reader reader(channel);
        int got = -1;

        // The loop sta_loop's worker runs: closed() is asked only when a receive came
        // back empty, and a close latched while the queue was still full is found on
        // the drain that follows.
        for (;;) {
            if (reader.receive(got)) {
                ++taken;
                continue;
            }

            if (channel.closed()) break;
        }

        while (reader.read(got)) ++taken;
    });

    // Enough to span several blocks, so the close arrives with elements still queued.
    for (int i = 0; i < 100; ++i) channel.send(i);

    channel.close();
    channel.signal(true);
    consumer.join();

    EXPECT_EQ(taken, 100);
    EXPECT_TRUE(channel.closed());
}

TEST(SpscChannelTest, AReaderAsleepAtCloseIsWokenAndFindsTheChannelClosed) {
    channel_t channel{false};  // auto-reset, the shape sta_loop uses
    std::latch asleep(1);
    bool found_closed = false;
    bool received_anything = false;

    std::thread consumer([&] {
        channel_t::reader reader(channel);
        int got = -1;

        asleep.count_down();

        for (;;) {
            if (reader.receive(got)) {
                received_anything = true;
                continue;
            }

            if (channel.closed()) {
                found_closed = true;
                break;
            }
        }
    });

    asleep.wait();
    channel.close();
    channel.signal(true);
    consumer.join();

    EXPECT_TRUE(found_closed);
    EXPECT_FALSE(received_anything);
}

TEST(SpscChannelTest, NothingIsSignalledWhileTheReaderIsNotWaiting) {
    // The writer's disarm() finds the trigger clear while the reader is awake, so
    // the primitive is never touched -- which is the whole reason the channel exists.
    // Visible through the trigger itself: a reader that has declared itself waiting is
    // what turns the next send into a signal.
    channel_t channel{false};  // auto-reset, the shape sta_loop uses
    channel_t::reader reader(channel);

    EXPECT_FALSE(channel.disarm());

    channel.arm();

    EXPECT_TRUE(channel.disarm());
    EXPECT_FALSE(channel.disarm());
}
