#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;



namespace {

using wxl::async::thread_scope;
using wxl::async::threaded_allocator;

// threaded_allocator::init() binds the calling OS thread's TLS slot and aborts on a second
// call from the same thread, so -- unlike sta_memory_pool, which is a process-wide singleton
// with its own gtest Environment -- every test here does its actual alloc()/free() work on a
// freshly spawned worker thread rather than gtest's shared runner thread, so tests never
// collide with each other's TLS state.

}  // namespace

TEST(ThreadedAllocatorTest, SingleThreadAllocFreeReuseAcrossPageRollover) {
    thread_scope manager;

    manager->spawn([] {
        threaded_allocator::init(4096);  // tiny page: forces free-list reuse and new pages.

        std::vector<void*> live;
        for (int round = 0; round < 5; ++round) {
            for (int i = 0; i < 2000; ++i) {
                void* p = threaded_allocator::alloc(24);
                std::memset(p, 0xAB, 24);
                live.push_back(p);
            }
            for (void* p : live) {
                EXPECT_EQ(static_cast<unsigned char*>(p)[0], 0xAB);
                threaded_allocator::free(p, 24);
            }
            live.clear();
        }
    });

    manager->join_all();
}

TEST(ThreadedAllocatorTest, DifferentSizeClassesDoNotAlias) {
    thread_scope manager;

    manager->spawn([] {
        threaded_allocator::init();

        void* a = threaded_allocator::alloc(24);
        void* b = threaded_allocator::alloc(240);
        void* c = threaded_allocator::alloc(2400);

        EXPECT_NE(a, b);
        EXPECT_NE(b, c);
        EXPECT_NE(a, c);

        std::memset(a, 1, 24);
        std::memset(b, 2, 240);
        std::memset(c, 3, 2400);

        EXPECT_EQ(static_cast<unsigned char*>(a)[0], 1);
        EXPECT_EQ(static_cast<unsigned char*>(b)[0], 2);
        EXPECT_EQ(static_cast<unsigned char*>(c)[0], 3);

        threaded_allocator::free(a, 24);
        threaded_allocator::free(b, 240);
        threaded_allocator::free(c, 2400);
    });

    manager->join_all();
}

// Sizes 1/2/4/8 all collapse onto the same 16-byte-total class (see s_alloc_table /
// SmallestClassIndex in threaded_allocator.h), so the bump allocator advances cursor_ by a
// constant 16 bytes for every one of them regardless of which of the four sizes was actually
// requested. Before that collapsing was in place, classes for 1/2/4 bytes alone would have
// advanced cursor_ by 9/10/12 bytes -- not multiples of sizeof(void*) -- walking every later
// allocation on the page off pointer alignment. Alternating the four sizes here means a
// pointer-alignment regression shows up as a misaligned owner-pointer/free-list write (a
// crash or a torn read) well before the loop completes.
TEST(ThreadedAllocatorTest, TinySizesCollapseOntoAlignedClassWithoutDriftingCursor) {
    thread_scope manager;

    manager->spawn([] {
        threaded_allocator::init(4096);

        const size_t sizes[] = {1, 2, 4, 8};
        std::vector<void*> live;

        for (int round = 0; round < 200; ++round) {
            for (size_t size : sizes) {
                void* p = threaded_allocator::alloc(size);
                ASSERT_EQ(reinterpret_cast<uintptr_t>(p) % alignof(void*), 0u);
                std::memset(p, 0x5A, size);
                live.push_back(p);
            }
        }

        for (size_t i = 0; i < live.size(); ++i) {
            const size_t size = sizes[i % 4];
            EXPECT_EQ(static_cast<unsigned char*>(live[i])[0], 0x5A);
            threaded_allocator::free(live[i], size);
        }
    });

    manager->join_all();
}

// The main (test-runner) thread frees blocks handed to it by a worker thread that keeps
// allocating on a tiny page behind its back, forcing that worker through
// drain-deferred-then-alloc-page repeatedly for real (not just in principle).
TEST(ThreadedAllocatorTest, CrossThreadFreeIsReclaimedByOwnerOnNextPageRollover) {
    constexpr int N = 20000;
    std::vector<void*> handed_off(N);
    std::atomic<int> produced{0};

    thread_scope manager;
    manager->spawn([&] {
        threaded_allocator::init(4096);
        for (int i = 0; i < N; ++i) {
            void* p = threaded_allocator::alloc(24);
            std::memset(p, 0xCD, 24);
            handed_off[i] = p;
            produced.store(i + 1, std::memory_order_release);
        }
        // Keep allocating after the main thread starts freeing behind its back, so the
        // worker's page actually runs dry while foreign frees are in flight.
        for (int i = 0; i < 5000; ++i) {
            void* p = threaded_allocator::alloc(24);
            threaded_allocator::free(p, 24);
        }
    });

    int consumed = 0;
    while (consumed < N) {
        const int avail = produced.load(std::memory_order_acquire);
        while (consumed < avail) {
            unsigned char* p = static_cast<unsigned char*>(handed_off[consumed]);
            EXPECT_EQ(p[0], 0xCD);
            threaded_allocator::free(p, 24);
            ++consumed;
        }
    }

    manager->join_all();
}

// A block can legitimately outlive the thread that allocated it: the owning thread exits
// without freeing everything it handed out, so the allocator must stay alive (kept alive by
// that block's own reference) until whichever thread eventually frees it -- here, long after
// the owning thread and its thread_group cleanup callback have already run to completion.
TEST(ThreadedAllocatorTest, AllocatorOutlivesOwnerThreadUntilLastBlockIsFreed) {
    for (int iter = 0; iter < 200; ++iter) {
        void* handed_off = nullptr;
        {
            thread_scope manager;
            manager->spawn([&] {
                threaded_allocator::init(4096);
                void* p = threaded_allocator::alloc(24);
                std::memset(p, 0xEF, 24);
                handed_off = p;
                // Deliberately does not free `p` -- the thread exits with it still outstanding.
            });
            manager->join_all();
        }

        ASSERT_NE(handed_off, nullptr);
        EXPECT_EQ(static_cast<unsigned char*>(handed_off)[0], 0xEF);
        threaded_allocator::free(handed_off, 24);  // releases the last reference; must not crash.
    }
}
