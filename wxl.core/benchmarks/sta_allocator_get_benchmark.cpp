// The cost of the allocation path alone -- the free list pop, the bump allocation, and the
// dispatch slot that decides which of the two a request reaches. Everything goes through
// alloc<Size>()/free<Size>(), never the runtime-sized entry points: with Size a template
// argument the class index is a compile-time constant, so none of the measurement is spent
// classifying the request.
//
// Four scenarios, because what a dispatch scheme costs depends entirely on whether the free
// list runs dry, and a scheme that wins on one of these usually pays for it on another. This is
// the instrument a proposed change to the scheme has to answer to; the rows have to be read
// together, and `fresh` first, since it is the one an application startup lives in:
//
//  1. pure pop -- the list is refilled outside the timed region, so every timed operation is a
//     pop from a list that is never empty and the scheme has nothing to decide. The floor.
//  2. balanced -- one free and one allocation per round. The list oscillates between one block
//     and none, and the pop always finds the block, so a scheme that pays on the free to save
//     on the allocation only pays.
//  3. growing -- one free and two allocations per round, which is what building a structure
//     looks like. The second allocation of every round finds the list empty, which is the only
//     place the scheme has a real decision to make. Its number carries the harness's own
//     push_back into the live-block vector, so read it as a comparison, not as a cost.
//  4. fresh -- allocations only, never a free, so every one of them comes off the page cursor.
//     The self-patching slot points straight at the bump allocator here and serves the whole
//     run without a test; a scheme that always entered through get() would pay that test on
//     every allocation and have no free to have saved anything on.
//
// Nothing is handed back at the end of a scenario, deliberately: each one leaves its class's
// free list empty, so the order they run in cannot colour the results. The blocks stay in the
// pool, which is what a pool is.
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

import wxl.core;

namespace {

using pool = wxl::core::sta_memory_pool;

constexpr uint32_t Size = 32;

double ns_per(std::chrono::steady_clock::duration d, uint64_t ops) {
    return std::chrono::duration<double, std::nano>(d).count() / static_cast<double>(ops);
}

// A compiler barrier, and where it sits matters: between the free and the allocation that
// follows it, never around the pair.
//
// A pool the optimizer can see into -- one whose dispatch is a direct call rather than a load
// from a table -- lets it prove that a free followed by an allocation of the same class hands
// back the same block and leaves the free list as it was, and it then deletes both. Measured,
// not feared: a pool built that way reported 0.00 ns per round here, and 0.27 with a plain
// barrier around the pair. A signal fence between the two makes the intermediate state of the
// free list observable, so the pair has to happen. It emits no instructions.
inline void barrier() noexcept { std::atomic_signal_fence(std::memory_order_seq_cst); }

double pure_pop(unsigned depth, unsigned rounds) {
    std::vector<void*> blocks(depth);
    for (unsigned i = 0; i < depth; ++i)
        blocks[i] = pool::alloc<Size>();

    std::chrono::steady_clock::duration total{};

    for (unsigned r = 0; r < rounds; ++r) {
        for (unsigned i = 0; i < depth; ++i)  // refill, outside the measurement
            pool::free<Size>(blocks[i]);

        const auto t0 = std::chrono::steady_clock::now();
        for (unsigned i = 0; i < depth; ++i) {
            blocks[i] = pool::alloc<Size>();
            barrier();
        }
        total += std::chrono::steady_clock::now() - t0;
    }

    return ns_per(total, uint64_t(depth) * rounds);
}

// The one live block travels through here, so that what the free is handed is a value the
// optimizer cannot follow back to the allocation that produced it.
volatile void* g_live = nullptr;

double balanced(unsigned rounds) {
    g_live = pool::alloc<Size>();

    const auto t0 = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < rounds; ++i) {
        pool::free<Size>(const_cast<void*>(g_live));
        barrier();
        g_live = pool::alloc<Size>();
        barrier();
    }
    const auto t1 = std::chrono::steady_clock::now();

    return ns_per(t1 - t0, rounds);
}

double growing(unsigned rounds) {
    std::vector<void*> live;
    live.reserve(2 * rounds + 1);
    live.push_back(pool::alloc<Size>());

    size_t head = 0;

    const auto t0 = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < rounds; ++i) {
        pool::free<Size>(live[head++]);
        barrier();
        live.push_back(pool::alloc<Size>());
        live.push_back(pool::alloc<Size>());
        barrier();
    }
    const auto t1 = std::chrono::steady_clock::now();

    return ns_per(t1 - t0, rounds);
}

// Allocations only. The blocks are kept, never handed back, which is the point: the free list
// stays empty for the whole run and every request has to come off the page cursor.
double fresh(unsigned count) {
    std::vector<void*> blocks(count);

    const auto t0 = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < count; ++i) {
        blocks[i] = pool::alloc<Size>();
        barrier();
    }
    const auto t1 = std::chrono::steady_clock::now();

    return ns_per(t1 - t0, count);
}

template <typename Fn>
double best_of(Fn run, int repeats) {
    double best = run();
    for (int r = 1; r < repeats; ++r)
        best = std::min(best, run());
    return best;
}

void report(const char* scenario, const char* unit, double ns) {
    std::printf("  %-9s %-28s %6.2f ns\n", scenario, unit, ns);
}

}  // namespace

int main() {
    constexpr int Repeats = 5;
    constexpr unsigned Depth = 4096;
    constexpr unsigned PopRounds = 64;
    constexpr unsigned BalancedRounds = 200'000;
    constexpr unsigned GrowingRounds = 20'000;
    constexpr unsigned FreshCount = 200'000;

#ifndef NDEBUG
    std::printf("NOTE: this is a Debug build - absolute numbers are inflated.\n\n");
#endif

    std::printf(
        "Allocation path only, %u-byte class, alloc<Size>() dispatch, best of %d.\n"
        "  pure pop : %u blocks refilled outside the timed region, %u rounds\n"
        "  balanced : 1 free + 1 alloc per round, %u rounds\n"
        "  growing  : 1 free + 2 alloc per round, %u rounds -- the second alloc of every round\n"
        "             finds the list empty\n"
        "  fresh    : %u allocations and no frees at all -- every one off the page cursor\n\n",
        Size, Repeats, Depth, PopRounds, BalancedRounds, GrowingRounds, FreshCount);

    const double growing_ns = best_of([] { return growing(GrowingRounds); }, Repeats);
    const double balanced_ns = best_of([] { return balanced(BalancedRounds); }, Repeats);
    const double pop_ns = best_of([] { return pure_pop(Depth, PopRounds); }, Repeats);
    const double fresh_ns = best_of([] { return fresh(FreshCount); }, Repeats);

    report("pure pop", "per pop", pop_ns);
    report("balanced", "per round (1 free, 1 alloc)", balanced_ns);
    report("growing", "per round (1 free, 2 alloc)", growing_ns);
    report("fresh", "per alloc, never freed", fresh_ns);

    return 0;
}
