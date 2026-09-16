// spin_lock's public API is expressed in terms of ssize_t, which lives in the library's
// own "abi.h" (macros and C typedefs cannot travel through `import`).
#include "abi.h"

#include <gtest/gtest.h>

import std;
import wxl.core;

using namespace wxl::core;

TEST(SpinLockTest, spin_lock_test)
{
#ifdef NDEBUG
    const size_t thread_count = 100, loop_count = 100000;
#else
    const size_t thread_count = 30, loop_count = 10000;
#endif

    std::atomic<ssize_t> lock_target{0};
    std::barrier gate(static_cast<ptrdiff_t>(thread_count));

    size_t volatile counter = 0;

    {
        std::vector<std::jthread> threads;
        threads.reserve(thread_count);

        // Every thread waits at the barrier for all the others, so they all hammer the
        // lock at once; the jthreads join when they go out of scope.
        for (size_t i = 0; i < thread_count; i++) {
            threads.emplace_back([&] {
                gate.arrive_and_wait();

                for (size_t n = 0; n < loop_count; n++) {
                    spin_lock guard(lock_target);  // comment this to have an error behaviour
                    counter++;
                    _mm_pause();  // to increase collision chance
                }
            });
        }
    }

    ASSERT_EQ(counter, thread_count * loop_count);
}
