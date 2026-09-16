#include <gtest/gtest.h>

import std;
import wxl.core;

using namespace wxl::core;

namespace {

/// Holds `m` until told to let go, so that the thread under test finds it taken.
class holder {
public:
    explicit holder(mutex & m) : mutex_(m) {
        thread_ = std::thread([this] {
            mutex_.acquire();
            taken_.release();
            may_release_.acquire();
            mutex_.release();
        });

        taken_.acquire();  // do not return until the mutex is actually held
    }

    ~holder() {
        may_release_.release();
        thread_.join();
    }

private:
    mutex & mutex_;
    semaphore taken_{0};
    semaphore may_release_{0};
    std::thread thread_;
};

TEST(MutexTest, TryAcquireTakesAFreeMutexAndRefusesATakenOne) {
    mutex m;

    ASSERT_TRUE(m.try_acquire());
    EXPECT_TRUE(m.is_synchronized());
    m.release();
    EXPECT_FALSE(m.is_synchronized());

    const holder other(m);

    EXPECT_FALSE(m.try_acquire());
    EXPECT_FALSE(m.is_synchronized());
}

TEST(MutexTest, AcquireWaitsForTheOwnerToLetGo) {
    // The slow path with no deadline: it keeps no timer and parks on the semaphore until
    // the mutex is free, however long that takes.
    mutex m;
    std::atomic<bool> acquired{false};
    std::thread waiter;

    {
        const holder other(m);

        waiter = std::thread([&m, &acquired] {
            m.acquire();
            acquired.store(true);
            m.release();
        });

        // Long enough for the waiter to run out of spins and park; it cannot have taken the
        // mutex in that time, because this scope still holds it.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        EXPECT_FALSE(acquired.load());
    }

    waiter.join();

    EXPECT_TRUE(acquired.load());
    EXPECT_TRUE(m.try_acquire());
    m.release();
}

TEST(MutexTest, TryAcquireForGivesUpWhenTheDeadlinePasses) {
    mutex m;
    const holder other(m);

    const time_stamp before = time_stamp::now();

    EXPECT_FALSE(m.try_acquire_for(duration::from_ms(50)));

    // It waited: giving up early would mean the deadline was not what it counted down to.
    EXPECT_GE((time_stamp::now() - before).total_milliseconds(), 40);
}

TEST(MutexTest, TryAcquireForTakesAFreeMutexWithoutWaiting) {
    mutex m;

    ASSERT_TRUE(m.try_acquire_for(duration::from_sec(30)));
    EXPECT_TRUE(m.is_synchronized());
    m.release();
}

TEST(RecursiveMutexTest, TheOwnerTakesItAgainWithoutBlocking) {
    recursive_mutex m;

    m.acquire();
    EXPECT_TRUE(m.is_synchronized());

    m.acquire();  // re-entry
    EXPECT_TRUE(m.try_acquire());
    EXPECT_TRUE(m.try_acquire_for(duration::zero()));

    m.release();
    m.release();
    m.release();

    EXPECT_TRUE(m.is_synchronized());  // still held once

    m.release();

    EXPECT_FALSE(m.is_synchronized());
}

TEST(RecursiveMutexTest, AnotherThreadWaitsForEveryLevelToBeReleased) {
    recursive_mutex m;
    std::atomic<int> depth{0};
    semaphore taken{0};
    semaphore may_release{0};

    std::thread owner([&] {
        m.acquire();
        m.acquire();
        depth.store(2);
        taken.release();
        may_release.acquire();
        m.release();
        depth.store(1);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        // The depth goes to zero before the release and not after it: the
        // release is what wakes the waiter, so a store after it is a store the
        // waiter can beat. The check below would then fail on a busy machine
        // for a reason that has nothing to do with the mutex -- while a waiter
        // that really did get in early still reads 1 and still fails it.
        depth.store(0);
        m.release();
    });

    taken.acquire();

    std::atomic<int> depth_when_acquired{-1};

    std::thread waiter([&] {
        m.acquire();
        depth_when_acquired.store(depth.load());
        m.release();
    });

    may_release.release();
    waiter.join();
    owner.join();

    EXPECT_EQ(depth_when_acquired.load(), 0);
}

}  // namespace
