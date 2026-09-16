// Compares plain new/delete against threaded_allocator-backed new/delete for a grid of
// object sizes (24 / 240 / 2400 bytes), in two scenarios:
//  1. single-threaded: allocate then free, all on one thread (same shape as
//     sta_allocator_benchmark.cpp).
//  2. two-thread ping-pong: two threads each create a batch of objects and hand every one of
//     them to the *other* thread to destroy, so every threaded_allocator object is freed on a
//     thread other than the one that allocated it -- exercising the cross-thread deferred-free
//     path instead of the same-thread fast path. Each thread's page is deliberately sized to
//     hold only a small slice of its batch, so the run forces many page rollovers and, with
//     them, many drain_deferred() calls that reclaim the other thread's foreign frees.
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <vector>

import wxl.async;

namespace {

using wxl::async::thread_group;
using wxl::async::thread_group_ptr;
using wxl::async::threaded_allocator;

template <size_t Size>
struct BaselineObject {
    std::byte pad[Size];
};

template <size_t Size>
struct ThreadedObject {
    std::byte pad[Size];

    static void* operator new(std::size_t sz) { return threaded_allocator::alloc(sz); }
    static void operator delete(void* p, std::size_t sz) noexcept { threaded_allocator::free(p, sz); }
};

struct Result {
    double alloc_ms = 0.0;
    double free_ms = 0.0;

    double total_ms() const { return alloc_ms + free_ms; }
};

// ---------------------------------------------------------------------------------------
// Scenario 1: single-threaded.

template <typename T>
Result run_once_single_threaded(unsigned count) {
    std::vector<T*> objects(count);

    auto t0 = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < count; ++i) {
        objects[i] = new T();
        objects[i]->pad[0] = std::byte{1};  // touch the memory, like real code would
    }
    auto t1 = std::chrono::steady_clock::now();

    for (unsigned i = 0; i < count; ++i)
        delete objects[i];
    auto t2 = std::chrono::steady_clock::now();

    return {
        std::chrono::duration<double, std::milli>(t1 - t0).count(),
        std::chrono::duration<double, std::milli>(t2 - t1).count(),
    };
}

template <typename T>
Result best_of_single_threaded(unsigned count, int repeats) {
    Result best;
    for (int i = 0; i < repeats; ++i) {
        Result r = run_once_single_threaded<T>(count);
        if (i == 0 || r.total_ms() < best.total_ms())
            best = r;
    }
    return best;
}

template <size_t Size>
void bench_single_threaded(const char* label, unsigned count, int repeats) {
    run_once_single_threaded<BaselineObject<Size>>(count);  // warm-up, discarded

    thread_group_ptr manager = thread_group::create();
    manager->spawn([&] {
        threaded_allocator::init(64 * 1024);
        run_once_single_threaded<ThreadedObject<Size>>(count);  // warm-up, discarded
    });
    manager->join_all();

    Result base = best_of_single_threaded<BaselineObject<Size>>(count, repeats);

    Result threaded;
    {
        thread_group_ptr bench_manager = thread_group::create();
        bench_manager->spawn([&] {
            threaded_allocator::init(64 * 1024);
            threaded = best_of_single_threaded<ThreadedObject<Size>>(count, repeats);
        });
        bench_manager->join_all();
    }

    std::printf("%-10s  size=%5zu  count=%6u\n", label, Size, count);
    std::printf("  new/delete       : alloc=%8.3f ms  free=%8.3f ms  total=%8.3f ms  (%7.1f ns/op)\n",
                base.alloc_ms, base.free_ms, base.total_ms(), base.total_ms() * 1e6 / (2.0 * count));
    std::printf("  threaded_alloc   : alloc=%8.3f ms  free=%8.3f ms  total=%8.3f ms  (%7.1f ns/op)\n",
                threaded.alloc_ms, threaded.free_ms, threaded.total_ms(),
                threaded.total_ms() * 1e6 / (2.0 * count));
    std::printf("  speedup          : %.2fx\n\n", base.total_ms() / threaded.total_ms());
}

// ---------------------------------------------------------------------------------------
// Scenario 2: two-thread ping-pong -- every object is freed by the thread that did *not*
// allocate it.

struct Channel {
    std::vector<void*> slots;
    std::atomic<size_t> produced{0};

    explicit Channel(size_t n) : slots(n) {}
};

// Consumes whatever is currently available in `in` without blocking.
template <typename T>
void drain_available(Channel& in, size_t& consumed) {
    const size_t avail = in.produced.load(std::memory_order_acquire);
    while (consumed < avail) {
        delete static_cast<T*>(in.slots[consumed]);
        ++consumed;
    }
}

// Spin-waits until `count` items total have been consumed from `in`.
template <typename T>
void drain_remaining(Channel& in, size_t& consumed, size_t count) {
    while (consumed < count)
        drain_available<T>(in, consumed);
}

// Both threads run the same interleaved shape -- allocate one, hand it to the other thread,
// then immediately reclaim whatever the other thread has sent back -- so an allocator that
// actually reclaims cross-thread frees as they arrive never needs to grow past its initial
// page, regardless of how large count_per_thread is. `setup` runs once at the start of each
// thread (used to call threaded_allocator::init() for the threaded_alloc variant; a no-op for
// the plain new/delete baseline, which needs none).
template <typename T, typename Setup>
double run_ping_pong(unsigned count_per_thread, Setup setup, size_t* out_pages_a, size_t* out_pages_b) {
    Channel a_to_b(count_per_thread), b_to_a(count_per_thread);

    auto t0 = std::chrono::steady_clock::now();

    thread_group_ptr manager = thread_group::create();
    manager->spawn([&] {
        setup();
        size_t consumed = 0;
        for (unsigned i = 0; i < count_per_thread; ++i) {
            auto* obj = new T();
            obj->pad[0] = std::byte{1};
            a_to_b.slots[i] = obj;
            a_to_b.produced.store(i + 1, std::memory_order_release);
            drain_available<T>(b_to_a, consumed);
        }
        drain_remaining<T>(b_to_a, consumed, count_per_thread);
        if (out_pages_a) {
            threaded_allocator* self = threaded_allocator::current();
            *out_pages_a = self ? self->page_count() : 0;
        }
    });
    manager->spawn([&] {
        setup();
        size_t consumed = 0;
        for (unsigned i = 0; i < count_per_thread; ++i) {
            auto* obj = new T();
            obj->pad[0] = std::byte{1};
            b_to_a.slots[i] = obj;
            b_to_a.produced.store(i + 1, std::memory_order_release);
            drain_available<T>(a_to_b, consumed);
        }
        drain_remaining<T>(a_to_b, consumed, count_per_thread);
        if (out_pages_b) {
            threaded_allocator* self = threaded_allocator::current();
            *out_pages_b = self ? self->page_count() : 0;
        }
    });
    manager->join_all();

    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// Baseline: plain new/delete needs no per-thread setup -- ordinary heap allocations are
// already safe to free from any thread.
template <size_t Size>
double run_baseline_ping_pong(unsigned count_per_thread) {
    return run_ping_pong<BaselineObject<Size>>(
        count_per_thread, [] {}, nullptr, nullptr);
}

// threaded_allocator: each thread's page is sized for a small, fixed in-flight window (not
// scaled by count_per_thread), just enough to prime the pipeline. From then on, since both
// threads reclaim what the other frees on essentially every iteration, alloc_impl() should
// satisfy every further request out of drain_deferred() alone -- i.e. VirtualAlloc should
// fire exactly once per thread (twice total) for the whole run, however large
// count_per_thread is. out_pages_a/out_pages_b report each thread's actual committed page
// count so that claim is checked, not just assumed.
template <size_t Size>
double run_threaded_ping_pong(unsigned count_per_thread, size_t* out_pages_a, size_t* out_pages_b) {
    constexpr size_t InFlightWindow = 256;
    const uint32_t page_size = static_cast<uint32_t>((Size + sizeof(void*)) * InFlightWindow);

    return run_ping_pong<ThreadedObject<Size>>(
        count_per_thread, [page_size] { threaded_allocator::init(page_size); }, out_pages_a, out_pages_b);
}

template <size_t Size>
void bench_ping_pong(const char* label, unsigned count_per_thread, int repeats) {
    size_t pages_a = 0, pages_b = 0;

    run_baseline_ping_pong<Size>(count_per_thread);                      // warm-up, discarded
    run_threaded_ping_pong<Size>(count_per_thread, &pages_a, &pages_b);  // warm-up, discarded

    double base_best = run_baseline_ping_pong<Size>(count_per_thread);
    for (int i = 1; i < repeats; ++i)
        base_best = std::min(base_best, run_baseline_ping_pong<Size>(count_per_thread));

    double threaded_best = run_threaded_ping_pong<Size>(count_per_thread, &pages_a, &pages_b);
    for (int i = 1; i < repeats; ++i) {
        size_t pa = 0, pb = 0;
        const double t = run_threaded_ping_pong<Size>(count_per_thread, &pa, &pb);
        if (t < threaded_best) {
            threaded_best = t;
            pages_a = pa;
            pages_b = pb;
        }
    }

    const unsigned total_objects = 2 * count_per_thread;
    std::printf("%-10s  size=%5zu  count/thread=%6u\n", label, Size, count_per_thread);
    std::printf("  new/delete       : total=%8.3f ms  (%7.1f ns/op)\n", base_best,
                base_best * 1e6 / (2.0 * total_objects));
    std::printf("  threaded_alloc   : total=%8.3f ms  (%7.1f ns/op)  [pages committed: A=%zu B=%zu]\n",
                threaded_best, threaded_best * 1e6 / (2.0 * total_objects), pages_a, pages_b);
    std::printf("  speedup          : %.2fx\n\n", base_best / threaded_best);
}

}  // namespace

int main() {
    constexpr unsigned N = 1000;
    constexpr int Repeats = 10;

#ifndef NDEBUG
    std::printf(
        "NOTE: this is a Debug build - the CRT debug heap makes plain new/delete\n"
        "artificially slow, which inflates the speedup below. Build Release for\n"
        "meaningful numbers.\n\n");
#endif

    std::printf("=== Scenario 1: single-threaded ===\n");
    std::printf("Grid: 24 bytes x %u, 240 bytes x %u, 2400 bytes x %u (%d repeats, best-of)\n\n",
                N * 16, N * 4, N, Repeats);

    bench_single_threaded<24>("24 bytes", N * 16, Repeats);
    bench_single_threaded<240>("240 bytes", N * 4, Repeats);
    bench_single_threaded<2400>("2400 bytes", N, Repeats);

    std::printf("=== Scenario 2: two-thread ping-pong (every object freed on the other thread) ===\n");
    std::printf("Grid per thread: 24 bytes x %u, 240 bytes x %u, 2400 bytes x %u (%d repeats, best-of)\n\n",
                N * 16, N * 4, N, Repeats);

    bench_ping_pong<24>("24 bytes", N * 16, Repeats);
    bench_ping_pong<240>("240 bytes", N * 4, Repeats);
    bench_ping_pong<2400>("2400 bytes", N, Repeats);

    return 0;
}
