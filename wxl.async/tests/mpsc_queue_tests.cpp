#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;



using namespace wxl::core;
using namespace wxl::async;

namespace {

struct test_node : intrusive_slist_node<test_node> {
    int value;
    explicit test_node(int v = 0) : value(v) {}
};

}  // namespace

static_assert(std::is_same_v<mpsc_stack<test_node>, mpsc_queue<test_node, drain_order::lifo>>,
              "mpsc_stack has to name the LIFO instantiation and nothing else");

TEST(MpscQueueTest, DefaultsToFifoAndReversesEachClaimedBatchToPushOrder) {
    mpsc_queue<test_node> queue;
    test_node a(1), b(2), c(3);

    queue.push_one(&a);
    queue.push_one(&b);
    queue.push_one(&c);

    EXPECT_EQ(queue.pop_one()->value, 1);
    EXPECT_EQ(queue.pop_one()->value, 2);
    EXPECT_EQ(queue.pop_one()->value, 3);
    EXPECT_EQ(queue.pop_one(), nullptr);
}

TEST(MpscQueueTest, LifoOrderHandsBackPushOrderReversed) {
    // The LIFO instantiation relays whatever pop_all() claims from the underlying stack,
    // which is last pushed, first out. `mpsc_stack` is the name for exactly this.
    mpsc_stack<test_node> queue;
    test_node a(1), b(2), c(3);

    queue.push_one(&a);
    queue.push_one(&b);
    queue.push_one(&c);

    EXPECT_EQ(queue.pop_one()->value, 3);
    EXPECT_EQ(queue.pop_one()->value, 2);
    EXPECT_EQ(queue.pop_one()->value, 1);
    EXPECT_EQ(queue.pop_one(), nullptr);
}

TEST(MpscQueueTest, OrderIsReversedPerBatchNotAcrossTheWholeRun) {
    mpsc_queue<test_node, drain_order::fifo> queue;
    test_node a(1), b(2), c(3), d(4);

    queue.push_one(&a);
    queue.push_one(&b);
    EXPECT_EQ(queue.pop_one()->value, 1);  // claims {a, b}, reverses it

    queue.push_one(&c);
    queue.push_one(&d);
    EXPECT_EQ(queue.pop_one()->value, 2);  // the first batch is not exhausted yet
    EXPECT_EQ(queue.pop_one()->value, 3);  // only now is {c, d} claimed and reversed
    EXPECT_EQ(queue.pop_one()->value, 4);
    EXPECT_EQ(queue.pop_one(), nullptr);
}

TEST(MpscQueueTest, PeekReturnsTheNextNodeWithoutRemovingIt) {
    mpsc_queue<test_node> queue;
    test_node a(1), b(2);

    queue.push_one(&a);
    queue.push_one(&b);

    EXPECT_EQ(queue.peek()->value, 1);
    EXPECT_EQ(queue.peek()->value, 1);  // still there, and still first
    EXPECT_EQ(queue.pops_since_claim(), 0u);  // peek() never advances the count

    EXPECT_EQ(queue.pop_one()->value, 1);
    EXPECT_EQ(queue.pop_one()->value, 2);
}

TEST(MpscQueueTest, PopsSinceClaimTracksPopsSinceTheLastBatchClaim) {
    mpsc_stack<test_node> queue;
    test_node a(1), b(2), c(3);

    queue.push_one(&a);
    queue.push_one(&b);
    EXPECT_EQ(queue.pops_since_claim(), 0u);

    queue.pop_one();
    EXPECT_EQ(queue.pops_since_claim(), 1u);
    queue.pop_one();
    EXPECT_EQ(queue.pops_since_claim(), 2u);

    // the batch is drained; the next pop_one() claims a fresh one and the counter
    // restarts from 1, it does not keep accumulating.
    queue.push_one(&c);
    queue.pop_one();
    EXPECT_EQ(queue.pops_since_claim(), 1u);
}

TEST(MpscQueueTest, EmptyReflectsBothTheReaderBatchAndTheUnderlyingStack) {
    mpsc_queue<test_node> queue;
    test_node a(1);

    EXPECT_TRUE(queue.empty());

    queue.push_one(&a);
    EXPECT_FALSE(queue.empty());  // sitting in the underlying stack, unclaimed

    queue.peek();
    EXPECT_FALSE(queue.empty());  // now sitting in the reader's own batch

    queue.pop_one();
    EXPECT_TRUE(queue.empty());
}

TEST(MpscQueueTest, PushOneOrderedDeliversTheSameWayPushOneDoes) {
    // The two differ only in the memory order the publishing CAS uses, so what a test can
    // hold them to is that the stronger one queues the element like the plain one.
    mpsc_queue<test_node> queue;
    test_node a(1), b(2);

    queue.push_one_ordered(&a);
    queue.push_one(&b);

    EXPECT_EQ(queue.pop_one()->value, 1);
    EXPECT_EQ(queue.pop_one()->value, 2);
    EXPECT_EQ(queue.pop_one(), nullptr);
}

TEST(MpscQueueTest, ClaimsEveryNodePushedConcurrentlyExactlyOnce) {
    mpsc_queue<test_node> queue;

    constexpr int writers = 8;
    constexpr int per_writer = 2000;
    constexpr int total = writers * per_writer;

    std::vector<test_node> nodes(total);
    for (int i = 0; i < total; ++i)
        nodes[i].value = i;

    std::vector<std::thread> threads;
    for (int w = 0; w < writers; ++w) {
        threads.emplace_back([&, w] {
            for (int i = 0; i < per_writer; ++i)
                queue.push_one(&nodes[w * per_writer + i]);
        });
    }
    for (auto& t : threads) t.join();

    std::unordered_set<int> seen;
    while (test_node* const node = queue.pop_one())
        EXPECT_TRUE(seen.insert(node->value).second) << "duplicate node " << node->value;

    EXPECT_EQ(seen.size(), static_cast<size_t>(total));
    EXPECT_TRUE(queue.empty());
}
