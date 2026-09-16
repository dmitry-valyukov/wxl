#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

/// Counts payloads alive, so that the tests about clear() and the destructor can say
/// whether an element was destroyed rather than merely dropped.
std::atomic<int> g_live_payloads{0};

struct payload : intrusive_slist_node<payload> {
    int value;

    explicit payload(int v = 0) : value(v) { g_live_payloads.fetch_add(1); }
    ~payload() { g_live_payloads.fetch_sub(1); }
};

using payload_ptr = std::unique_ptr<payload>;
using test_channel = mpsc_channel<payload>;

payload_ptr make(int value) { return std::make_unique<payload>(value); }

}  // namespace

TEST(MpscChannelTest, AnEmptyChannelHandsBackNothing) {
    test_channel channel;
    payload_ptr taken;

    EXPECT_FALSE(channel.try_receive(taken));
    EXPECT_EQ(taken, nullptr);

    // The same question through the timed receive, with nothing to spare: it has to
    // answer rather than wait.
    EXPECT_FALSE(channel.receive_for(taken, duration::zero()));
    EXPECT_EQ(taken, nullptr);
}

TEST(MpscChannelTest, OwnershipTravelsWithTheElement) {
    test_channel channel;
    payload_ptr sent = make(7);
    const payload* const address = sent.get();

    channel.send(sent);
    EXPECT_EQ(sent, nullptr) << "send() takes the element out of the caller's pointer";

    payload_ptr taken;
    ASSERT_TRUE(channel.try_receive(taken));
    EXPECT_EQ(taken.get(), address);
    EXPECT_EQ(taken->value, 7);
}

TEST(MpscChannelTest, ElementsComeOutInTheOrderTheyWereSent) {
    test_channel channel;

    for (int i = 1; i <= 3; ++i) {
        payload_ptr sent = make(i);
        channel.send(sent);
    }

    payload_ptr taken;
    for (int i = 1; i <= 3; ++i) {
        ASSERT_TRUE(channel.try_receive(taken));
        EXPECT_EQ(taken->value, i);
    }

    EXPECT_FALSE(channel.try_receive(taken));
}

TEST(MpscChannelTest, ReceiveComesBackWhenTheTimeoutElapses) {
    test_channel channel;
    payload_ptr taken;

    const auto started = std::chrono::steady_clock::now();
    EXPECT_FALSE(channel.receive_for(taken, duration::from_ms(30)));
    const auto waited = std::chrono::steady_clock::now() - started;

    // The lower bound only proves it waited rather than fell straight through; the point
    // of the test is that a finite timeout comes back at all.
    EXPECT_GE(waited, std::chrono::milliseconds(20));
}

TEST(MpscChannelTest, AWaitingReaderIsWokenBySend) {
    test_channel channel;
    std::barrier sync_point(2);
    payload_ptr taken;

    std::thread reader([&] {
        sync_point.arrive_and_wait();

        // The condition-variable contract: an empty-handed return is ordinary, so the
        // reader loops on its own predicate instead of trusting one answer.
        while (!channel.receive(taken)) {}
    });

    // The barrier only makes it likely that the reader is asleep by the time we send --
    // nothing can make it certain, and the loop above is why nothing has to.
    sync_point.arrive_and_wait();

    payload_ptr sent = make(42);
    channel.send(sent);

    reader.join();

    ASSERT_NE(taken, nullptr);
    EXPECT_EQ(taken->value, 42);
}

TEST(MpscChannelTest, AForcedSignalWakesAReaderWithNothingToTake) {
    // How an owner stops a reader parked on an infinite wait: raise its flag, then
    // signal(true). The reader wakes empty-handed and looks at the flag itself -- the
    // channel has nothing to tell it apart from "look again".
    test_channel channel;
    std::barrier sync_point(2);
    std::atomic<bool> stop{false};
    std::atomic<bool> stopped_on_its_own_flag{false};

    std::thread reader([&] {
        payload_ptr taken;
        sync_point.arrive_and_wait();

        while (!channel.receive(taken)) {
            if (stop.load()) {
                stopped_on_its_own_flag.store(true);
                return;
            }
        }
    });

    sync_point.arrive_and_wait();

    stop.store(true);
    channel.signal(true);

    reader.join();

    EXPECT_TRUE(stopped_on_its_own_flag.load());
}

TEST(MpscChannelTest, ALeftoverSignalComesBackAsASpuriousWakeup) {
    // A forced signal with nothing behind it sits in the auto-reset event until somebody
    // waits on it, so the next receive() comes back empty-handed long before its timeout.
    // That is ordinary under the condition-variable contract -- and it is exactly why a
    // reader must loop instead of reading a `false` as "the timeout elapsed".
    test_channel channel;
    payload_ptr taken;

    channel.signal(true);

    const auto started = std::chrono::steady_clock::now();
    EXPECT_FALSE(channel.receive_for(taken, duration::from_ms(10000)));
    const auto waited = std::chrono::steady_clock::now() - started;

    EXPECT_LT(waited, std::chrono::seconds(5)) << "the leftover signal was lost";

    // And the reader is none the worse for it: the next send still arrives.
    payload_ptr sent = make(5);
    channel.send(sent);

    ASSERT_TRUE(channel.try_receive(taken));
    EXPECT_EQ(taken->value, 5);
}

TEST(MpscChannelTest, EmptyAnswersTheReadersQuestion) {
    test_channel channel;

    EXPECT_TRUE(channel.empty());

    payload_ptr sent = make(1);
    channel.send(sent);

    EXPECT_FALSE(channel.empty());

    payload_ptr taken;
    ASSERT_TRUE(channel.try_receive(taken));

    EXPECT_TRUE(channel.empty());
}

TEST(MpscChannelTest, PeekShowsTheNextElementWithoutTakingIt) {
    test_channel channel;
    payload_ptr sent = make(3);
    channel.send(sent);

    ASSERT_NE(channel.peek(), nullptr);
    EXPECT_EQ(channel.peek()->value, 3);
    EXPECT_EQ(channel.peek()->value, 3) << "peek() leaves the element where it is";
    EXPECT_FALSE(channel.empty());

    payload_ptr taken;
    ASSERT_TRUE(channel.try_receive(taken));
    EXPECT_EQ(taken->value, 3);
    EXPECT_EQ(channel.peek(), nullptr);
}

TEST(MpscChannelTest, PopOneHandsTheRawElementAndItsOwnershipOver) {
    test_channel channel;
    payload_ptr sent = make(4);
    const payload* const address = sent.get();
    channel.send(sent);

    // Nothing owns the element between the two lines; that is the difference from
    // try_receive(), and the reason a caller needs a place to put it straight away.
    payload_ptr taken(channel.pop_one());

    ASSERT_NE(taken, nullptr);
    EXPECT_EQ(taken.get(), address);
    EXPECT_EQ(channel.pop_one(), nullptr);
}

TEST(MpscChannelTest, TheWaitObjectIsTheOneTheReaderSleepsOn) {
    // An owner that has to wait on more than this channel at once takes the primitive
    // itself -- WaitForMultipleObjects and its like want handles, not channels.
    test_channel channel;

    ASSERT_NE(channel.get_wait_object(), nullptr);
    EXPECT_EQ(channel.wait_handle(), channel.get_wait_object()->handle());
    EXPECT_FALSE(channel.get_wait_object()->wait_for(duration::zero())) << "nothing has happened yet";

    channel.arm();

    payload_ptr sent = make(1);
    channel.send(sent);

    EXPECT_TRUE(channel.get_wait_object()->wait_for(duration::zero())) << "send() signalled the wait object";

    payload_ptr taken;
    ASSERT_TRUE(channel.try_receive(taken));
    EXPECT_EQ(taken->value, 1);
}

TEST(MpscChannelTest, WritersSignalOnlyWhileTheReaderSaysItIsWaiting) {
    // The whole point of the trigger: no kernel call per element while the reader is
    // busy working through what it already has.
    test_channel channel;

    channel.signal();
    EXPECT_FALSE(channel.get_wait_object()->wait_for(duration::zero())) << "nobody announced a wait";

    channel.arm();
    channel.signal();
    EXPECT_TRUE(channel.get_wait_object()->wait_for(duration::zero()));

    // signal() cleared the trigger on its way through, so the next writer finds nobody
    // waiting again.
    EXPECT_FALSE(channel.disarm());

    // And the reader clears its own trigger when it stops waiting, which is what the
    // return value reports.
    channel.arm();
    EXPECT_TRUE(channel.disarm());
    EXPECT_FALSE(channel.disarm());
}

TEST(MpscChannelTest, ClearDestroysWhatIsStillInside) {
    const int live_before = g_live_payloads.load();
    test_channel channel;

    for (int i = 0; i < 3; ++i) {
        payload_ptr sent = make(i);
        channel.send(sent);
    }

    EXPECT_EQ(g_live_payloads.load(), live_before + 3);

    channel.clear();

    EXPECT_EQ(g_live_payloads.load(), live_before);

    payload_ptr taken;
    EXPECT_FALSE(channel.try_receive(taken));
}

TEST(MpscChannelTest, TheDestructorDropsWhatWasNeverTaken) {
    const int live_before = g_live_payloads.load();

    {
        test_channel channel;
        payload_ptr sent = make(1);
        channel.send(sent);

        EXPECT_EQ(g_live_payloads.load(), live_before + 1);
    }

    EXPECT_EQ(g_live_payloads.load(), live_before);
}

TEST(MpscChannelTest, ManyWritersOneReaderLoseNothing) {
    constexpr int writer_count = 8;
    constexpr int per_writer = 500;

    test_channel channel;
    std::barrier sync_point(writer_count);
    std::atomic<long long> sent_sum{0};
    std::vector<std::thread> writers;
    writers.reserve(writer_count);

    for (int w = 0; w < writer_count; ++w) {
        writers.emplace_back([&, w] {
            sync_point.arrive_and_wait();

            for (int i = 0; i < per_writer; ++i) {
                payload_ptr sent = make(w * per_writer + i);
                sent_sum.fetch_add(sent->value);  // send() empties the pointer.
                channel.send(sent);
            }
        });
    }

    long long received_sum = 0;
    payload_ptr taken;

    for (int i = 0; i < writer_count * per_writer; ++i) {
        while (!channel.receive(taken)) {}
        received_sum += taken->value;
    }

    for (auto& writer : writers) writer.join();

    EXPECT_EQ(received_sum, sent_sum.load());
    EXPECT_FALSE(channel.try_receive(taken));
}
