// Isolates sta_memory_pool's pure bump-allocator cost from everything else that can show up
// in ns/op: no VirtualAlloc during either timed loop (the page is pre-sized for the whole
// run), no free-list involvement (nothing is ever freed), and -- unlike the other benchmarks
// in this directory, which touch each object's first byte to mimic real code -- no memory
// access at all. Dereferencing the returned address to touch memory is a *separate* cost
// (page faults on first touch, cache/TLB effects) that has nothing to do with the allocator
// itself, so this test deliberately never performs it. Returned addresses are summed into a
// checksum *by value* (never by dereferencing) purely so the compiler cannot prove either
// loop's results are unused and discard it -- addition rather than XOR, since XOR-ing a
// constant-stride sequence from a page-aligned base is prone to landing on exactly 0 for many
// N, which is harmless but an unnecessarily confusing thing to stare at.
//
// Two scenarios, both timing allocation only (no construction, no free):
//  1. generic dispatch: sta_memory_pool::alloc(size_t) -- size is an ordinary runtime
//     argument, so pool_index(size) cannot be evaluated as a constant expression, and the
//     generated code computes it for real on every call (see the investigation that led
//     here: even a literal argument doesn't get folded, because MSVC's std::countl_zero
//     doesn't have constant-folding support for the underlying lzcnt/bsr intrinsics -- only
//     forcing evaluation through the language's constant-expression rules does).
//  2. CRTP operator new: StaObject<Derived>::operator new ignores its runtime size_t
//     parameter and calls sta_memory_pool::alloc<sizeof(Derived)>() instead, where Size is a
//     genuine template argument. pool_index(Size) is computed via a constexpr local, so the
//     whole per-class-dispatch step collapses to a single indirect call through a
//     compile-time-fixed table offset -- confirmed by disassembly during the investigation,
//     not just asserted here.
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>

import wxl.core;

using wxl::core::sta_memory_pool;

namespace {

struct Object64 {
    std::byte pad[64];
};

// The CRTP base in question: looks like "one shared base class" to callers, but is actually a
// distinct template instantiation -- and hence a distinct, separately compiled operator
// new/delete -- per Derived, giving each one sizeof(Derived) as a compile-time constant.
template <typename Derived>
class StaObject {
public:
    static void* operator new(std::size_t /*size*/) {
        return sta_memory_pool::alloc<static_cast<uint32_t>(sizeof(Derived))>();
    }

    static void operator delete(void* p, std::size_t /*size*/) noexcept {
        sta_memory_pool::free<static_cast<uint32_t>(sizeof(Derived))>(p);
    }
};

struct TemplatedObject64 : StaObject<TemplatedObject64> {
    std::byte pad[64];
};

// StaObject<Derived> is empty (no data members, no virtual functions), so the empty base
// optimization should keep this exactly comparable in size to Object64; guard that assumption
// instead of silently comparing allocations of different sizes if it ever stops holding.
static_assert(sizeof(TemplatedObject64) == sizeof(Object64));

double ns_per_op(std::chrono::steady_clock::duration d, unsigned ops) {
    return std::chrono::duration<double, std::nano>(d).count() / ops;
}

}  // namespace

int main() {
    constexpr unsigned N = 10'000;  // allocations timed per repeat, per scenario
    constexpr int Repeats = 5;
#ifndef NDEBUG
    std::printf("NOTE: this is a Debug build - absolute numbers are inflated.\n\n");
#endif

    std::printf(
        "Pure bump-allocator scenarios: size=%zu bytes, N=%u allocations per repeat, %d repeats\n"
        "Page pre-sized to %u bytes for the whole run (no VirtualAlloc during either timed\n"
        "loop); nothing is ever freed (no free-list traversal); returned pointers are never\n"
        "dereferenced (no page faults, no cache/TLB effects from touching memory).\n\n",
        sizeof(Object64), N, Repeats, sta_memory_pool::PageSize);

    double best_generic_ns = 1e300;
    std::printf("1) generic dispatch -- sta_memory_pool::alloc(size_t):\n");
    for (int r = 0; r < Repeats; ++r) {
        uintptr_t checksum = 0;

        const auto t0 = std::chrono::steady_clock::now();
        for (unsigned i = 0; i < N; ++i) {
            void* p = sta_memory_pool::alloc(sizeof(Object64));
            checksum += reinterpret_cast<uintptr_t>(p);  // by value only -- never dereferenced
        }
        const auto t1 = std::chrono::steady_clock::now();

        const double ns = ns_per_op(t1 - t0, N);
        std::printf("  repeat %d: %8.3f ns/op  (checksum: %llx)\n", r + 1, ns,
                     static_cast<unsigned long long>(checksum));
        if (ns < best_generic_ns)
            best_generic_ns = ns;
    }
    std::printf("  best: %.3f ns/op\n\n", best_generic_ns);

    double best_crtp_ns = 1e300;
    std::printf("2) CRTP operator new -- sta_memory_pool::alloc<sizeof(Derived)>():\n");
    for (int r = 0; r < Repeats; ++r) {
        uintptr_t checksum = 0;

        const auto t0 = std::chrono::steady_clock::now();
        for (unsigned i = 0; i < N; ++i) {
            void* p = TemplatedObject64::operator new(sizeof(TemplatedObject64));
            checksum += reinterpret_cast<uintptr_t>(p);  // by value only -- never dereferenced
        }
        const auto t1 = std::chrono::steady_clock::now();

        const double ns = ns_per_op(t1 - t0, N);
        std::printf("  repeat %d: %8.3f ns/op  (checksum: %llx)\n", r + 1, ns,
                     static_cast<unsigned long long>(checksum));
        if (ns < best_crtp_ns)
            best_crtp_ns = ns;
    }
    std::printf("  best: %.3f ns/op\n\n", best_crtp_ns);

    std::printf("speedup: %.2fx (generic dispatch / CRTP templated dispatch)\n",
                best_generic_ns / best_crtp_ns);

    return 0;
}
