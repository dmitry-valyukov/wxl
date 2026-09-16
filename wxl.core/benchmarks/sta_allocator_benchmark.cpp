// Compares plain new/delete against sta_allocator-backed new/delete for a grid of object sizes
// (24 / 240 / 2400 bytes) and counts, using each object's own operator new/operator delete so
// the measurement reflects how the allocators are actually invoked in real code.
//
// The shape here is allocate-everything-then-free-everything, which is the pool's easiest
// case: the whole allocation phase runs off the bump cursor and the whole free phase pushes.
// sta_allocator_scenario_benchmark.cpp is the counterpart where frees and allocations
// interleave, which is what real code does.
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <vector>

import wxl.core;

namespace {

template <typename T>
using sta_allocator = wxl::core::sta_allocator<T>;

template <size_t Size>
struct BaselineObject {
    std::byte pad[Size];
};

template <size_t Size, template <typename> class Alloc>
struct PoolObject {
    std::byte pad[Size];

    static void* operator new(std::size_t) { return Alloc<PoolObject>{}.allocate(1); }

    static void operator delete(void* p, std::size_t) noexcept {
        Alloc<PoolObject>{}.deallocate(static_cast<PoolObject*>(p), 1);
    }
};

struct Result {
    double alloc_ms = 0.0;
    double free_ms = 0.0;

    double total_ms() const { return alloc_ms + free_ms; }
};

template <typename T>
Result run_once(unsigned count) {
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
Result best_of(unsigned count, int repeats) {
    Result best;
    for (int i = 0; i < repeats; ++i) {
        Result r = run_once<T>(count);
        if (i == 0 || r.total_ms() < best.total_ms())
            best = r;
    }
    return best;
}

void print_line(const char* label, const Result& r, unsigned count) {
    std::printf("  %-12s: alloc=%8.3f ms  free=%8.3f ms  total=%8.3f ms  (%7.1f ns/op)\n", label,
                r.alloc_ms, r.free_ms, r.total_ms(), r.total_ms() * 1e6 / (2.0 * count));
}

template <size_t Size>
void bench_size(const char* label, unsigned count, int repeats) {
    using Sta = PoolObject<Size, sta_allocator>;

    run_once<BaselineObject<Size>>(count);  // warm-up, discarded
    run_once<Sta>(count);

    Result base = best_of<BaselineObject<Size>>(count, repeats);
    Result sta = best_of<Sta>(count, repeats);

    std::printf("%-10s  size=%5zu  count=%6u\n", label, Size, count);
    print_line("new/delete", base, count);
    print_line("sta_alloc", sta, count);
    std::printf("  speedup     : %.2fx over new/delete\n\n", base.total_ms() / sta.total_ms());
}

// ---------------------------------------------------------------------------------------
// Recycling: a fixed working set of live objects, each slot freed and immediately allocated
// again, so every allocation is served off the free list a free has just pushed to and nothing
// else happens in the loop. The grid above never puts a free in front of an allocation, so this
// is the only place here where the free list is exercised at all rather than just filled.
//
// `new T` without the parentheses, unlike the grid: value-initialising a 2400-byte object
// memsets it, which at these sizes costs several times the allocation and hides everything
// this scenario is here to see.

template <typename T>
double run_recycle(unsigned working_set, unsigned iterations) {
    std::vector<T*> objects(working_set);
    for (unsigned i = 0; i < working_set; ++i)
        objects[i] = new T;

    const auto t0 = std::chrono::steady_clock::now();
    for (unsigned i = 0, slot = 0; i < iterations; ++i) {
        delete objects[slot];
        objects[slot] = new T;

        if (++slot == working_set)
            slot = 0;
    }
    const auto t1 = std::chrono::steady_clock::now();

    for (unsigned i = 0; i < working_set; ++i)
        delete objects[i];

    return std::chrono::duration<double, std::nano>(t1 - t0).count() / iterations;
}

template <typename T>
double best_recycle(unsigned working_set, unsigned iterations, int repeats) {
    double best = run_recycle<T>(working_set, iterations);
    for (int i = 1; i < repeats; ++i)
        best = std::min(best, run_recycle<T>(working_set, iterations));
    return best;
}

template <size_t Size>
void bench_recycle(const char* label, unsigned working_set, unsigned iterations, int repeats) {
    using Sta = PoolObject<Size, sta_allocator>;

    run_recycle<BaselineObject<Size>>(working_set, iterations);  // warm-up, discarded
    run_recycle<Sta>(working_set, iterations);

    const double base = best_recycle<BaselineObject<Size>>(working_set, iterations, repeats);
    const double sta = best_recycle<Sta>(working_set, iterations, repeats);

    std::printf("%-10s  size=%5zu  working set=%5u  iterations=%7u\n", label, Size, working_set,
                iterations);
    std::printf("  %-12s: %6.2f ns per free+alloc pair\n", "new/delete", base);
    std::printf("  %-12s: %6.2f ns per free+alloc pair\n", "sta_alloc", sta);
    std::printf("  speedup     : %.2fx over new/delete\n\n", base / sta);
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

    std::printf("=== Allocate all, then free all ===\n");
    std::printf("Grid: 24 bytes x %u, 240 bytes x %u, 2400 bytes x %u (%d repeats, best-of)\n\n",
                N * 16, N * 4, N, Repeats);

    bench_size<24>("24 bytes", N * 16, Repeats);
    bench_size<240>("240 bytes", N * 4, Repeats);
    bench_size<2400>("2400 bytes", N, Repeats);

    std::printf("=== Recycling: every allocation follows a free of its own size class ===\n\n");

    bench_recycle<24>("24 bytes", 64, N * 100, Repeats);
    bench_recycle<24>("24 bytes", 1, N * 100, Repeats);
    bench_recycle<240>("240 bytes", 64, N * 100, Repeats);
    bench_recycle<2400>("2400 bytes", 64, N * 100, Repeats);

    return 0;
}
