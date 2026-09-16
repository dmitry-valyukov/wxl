#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;



using namespace wxl::core;
using namespace wxl::async;

TEST(CompletionCounterTest, TheOwnersOwnPlaceKeepsTheFutureUnresolved) {
    completion_counter counter;  // expected = 1: the owner holds a place

    const future<void> completed = counter.on_all_completed();
    EXPECT_FALSE(completed.is_ready());

    counter.complete_one();  // the owner gives its place up
    EXPECT_TRUE(completed.is_ready());
}

TEST(CompletionCounterTest, TheFutureResolvesOnlyAfterTheLastOperationRetires) {
    completion_counter counter;
    const future<void> completed = counter.on_all_completed();

    counter.start_one();
    counter.start_one();
    counter.complete_one();  // the owner leaves; two operations are still outstanding

    EXPECT_FALSE(completed.is_ready());

    counter.complete_one();
    EXPECT_FALSE(completed.is_ready());

    counter.complete_one();
    EXPECT_TRUE(completed.is_ready());
}

TEST(CompletionCounterTest, AnExplicitExpectedCountIsHonoured) {
    completion_counter counter(3);
    const future<void> completed = counter.on_all_completed();

    counter.complete_one();
    counter.complete_one();
    EXPECT_FALSE(completed.is_ready());

    counter.complete_one();
    EXPECT_TRUE(completed.is_ready());
}

TEST(CompletionCounterTest, AFutureTakenAfterTheCounterFiredIsAlreadyReady) {
    completion_counter counter;
    counter.complete_one();

    EXPECT_TRUE(counter.on_all_completed().is_ready());
}

TEST(CompletionCounterTest, EveryFutureHandedOutSeesTheSameCompletion) {
    completion_counter counter;

    const future<void> first = counter.on_all_completed();
    const future<void> second = counter.on_all_completed();

    EXPECT_FALSE(first.is_ready());
    EXPECT_FALSE(second.is_ready());

    counter.complete_one();

    EXPECT_TRUE(first.is_ready());
    EXPECT_TRUE(second.is_ready());
}

TEST(CompletionCounterTest, WorkStartedOnManyThreadsResolvesTheFutureExactlyOnceAtTheEnd) {
    constexpr int workers = 8;
    constexpr int per_worker = 500;

    completion_counter counter;
    const future<void> completed = counter.on_all_completed();
    std::atomic<int> retired{0};

    std::vector<std::jthread> threads;
    threads.reserve(workers);

    for (int w = 0; w < workers; ++w) {
        threads.emplace_back([&] {
            for (int i = 0; i < per_worker; ++i) {
                counter.start_one();
                retired.fetch_add(1, std::memory_order_relaxed);
                counter.complete_one();
            }
        });
    }

    threads.clear();  // joins

    // The owner still holds its place, so nothing above could have fired the future.
    EXPECT_FALSE(completed.is_ready());
    EXPECT_EQ(retired.load(), workers * per_worker);

    counter.complete_one();
    EXPECT_TRUE(completed.is_ready());
}

TEST(CompletionCounterGuardTest, TakesAPlaceOnConstructionAndGivesItBackOnDestruction) {
    completion_counter counter;
    const future<void> completed = counter.on_all_completed();

    {
        const completion_counter_guard guard(counter);
        counter.complete_one();  // the owner leaves while the guard holds the last place

        EXPECT_FALSE(completed.is_ready());
    }

    EXPECT_TRUE(completed.is_ready());
}

TEST(CompletionCounterGuardTest, NestedGuardsEachHoldTheirOwnPlace) {
    completion_counter counter;
    const future<void> completed = counter.on_all_completed();

    {
        const completion_counter_guard outer(counter);
        {
            const completion_counter_guard inner(counter);
            counter.complete_one();  // the owner leaves; two guards are still holding
            EXPECT_FALSE(completed.is_ready());
        }

        EXPECT_FALSE(completed.is_ready());
    }

    EXPECT_TRUE(completed.is_ready());
}
