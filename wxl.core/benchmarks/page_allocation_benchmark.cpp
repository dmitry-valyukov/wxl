// What one page of arena-block size costs to take and give back. The size is
// fixed at 1 MiB because that is the order of what a book-sized document needs
// (wxl.xml's arena holds 120 bytes per node, and a real book parses to some
// ten thousand of them), and the question this answers is whether a page that
// size can be taken per document without thinking about it.
//
// Three allocators, which are the three wxl can reach:
//
//  * std::malloc / std::free -- the CRT heap, which is the process heap.
//  * VirtualAlloc / VirtualFree -- pages straight from the kernel, which is
//    also where sta_memory_pool gets its own pages.
//  * sta_memory_pool -- the STA pool. A request this large is past its size
//    classes (32 KiB), so what actually serves it depends on the page the pool
//    was built with: above half a page it forwards to malloc, below it goes to
//    the pool's private HEAP_NO_SERIALIZE heap. The pool here is built with an
//    8 MiB page precisely so that a 1 MiB request lands on that private heap,
//    which is the interesting case -- with the default 1 MiB page the pool is
//    literally malloc, and measuring it would measure the first row twice.
//
// Two ways of asking, because they answer different questions:
//
//  * "take and give back" -- the allocation on its own. Every allocator's best
//    case, since the block just freed is the one handed back next.
//  * "take, write, give back" -- every 4 KiB page touched once before the
//    block goes back. Fresh address space is not backed until it is written,
//    so this is where a soft page fault per page shows up, and it is the
//    honest number for an arena block, which gets filled with nodes.
//
// And a third scenario for the cold case: sixteen blocks held at once, so the
// allocator cannot hand back what it has just taken in.

#include <windows.h>

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

import wxl.core;

using wxl::core::sta_memory_pool;

namespace {

using clock_type = std::chrono::steady_clock;

constexpr std::size_t page_bytes = 1024 * 1024;
constexpr std::size_t os_page = 4096;
constexpr std::size_t held_at_once = 16;

/// The smallest of several runs: the interesting quantity is what the machine
/// does when nothing interrupts it, and noise only ever adds.
class best_time {
public:
    void add(const clock_type::duration duration) noexcept {
        const double nanoseconds = std::chrono::duration<double, std::nano>(duration).count();

        if (nanoseconds < best_) best_ = nanoseconds;
    }

    double per_op(const std::size_t ops) const noexcept { return best_ / static_cast<double>(ops); }

private:
    double best_ = 1e30;
};

/// Writes one byte in every OS page of the block, which is what makes the
/// address space actually backed by memory. Returns something derived from
/// what it wrote so the loop cannot be optimised away.
std::size_t touch(void* const memory) noexcept {
    auto* const bytes = static_cast<volatile unsigned char*>(memory);
    std::size_t seen = 0;

    for (std::size_t at = 0; at < page_bytes; at += os_page) {
        bytes[at] = static_cast<unsigned char>(at);
        seen += bytes[at];
    }

    return seen;
}

// ---- the three allocators, each a pair of functions ----

void* malloc_take() noexcept { return std::malloc(page_bytes); }
void malloc_give(void* const memory) noexcept { std::free(memory); }

void* virtual_take() noexcept {
    return ::VirtualAlloc(nullptr, page_bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}
void virtual_give(void* const memory) noexcept { ::VirtualFree(memory, 0, MEM_RELEASE); }

void* pool_take() noexcept { return sta_memory_pool::alloc(page_bytes); }
void pool_give(void* const memory) noexcept { sta_memory_pool::free(memory, page_bytes); }

using take_fn = void* (*)() noexcept;
using give_fn = void (*)(void*) noexcept;

struct allocator {
    const char* name;
    take_fn take;
    give_fn give;
};

constexpr allocator allocators[] = {
    {"malloc/free", malloc_take, malloc_give},
    {"VirtualAlloc", virtual_take, virtual_give},
    {"STA pool", pool_take, pool_give},
};

// ---- the three scenarios ----

std::size_t checksum = 0;
std::size_t operations = 0;

double take_and_give(const allocator& what, const unsigned ops, const unsigned rounds) {
    best_time best;

    for (unsigned round = 0; round < rounds; ++round) {
        const auto started = clock_type::now();

        for (unsigned i = 0; i < ops; ++i) {
            void* const memory = what.take();
            checksum += reinterpret_cast<std::uintptr_t>(memory) >> 12;
            what.give(memory);
            ++operations;
        }

        best.add(clock_type::now() - started);
    }

    return best.per_op(ops);
}

double take_write_give(const allocator& what, const unsigned ops, const unsigned rounds) {
    best_time best;

    for (unsigned round = 0; round < rounds; ++round) {
        const auto started = clock_type::now();

        for (unsigned i = 0; i < ops; ++i) {
            void* const memory = what.take();
            checksum += touch(memory);
            what.give(memory);
            ++operations;
        }

        best.add(clock_type::now() - started);
    }

    return best.per_op(ops);
}

double sixteen_at_once(const allocator& what, const unsigned ops, const unsigned rounds) {
    best_time best;
    void* held[held_at_once]{};

    for (unsigned round = 0; round < rounds; ++round) {
        const auto started = clock_type::now();

        for (unsigned i = 0; i < ops; i += held_at_once) {
            for (void*& one : held) {
                one = what.take();
                checksum += reinterpret_cast<std::uintptr_t>(one) >> 12;
                ++operations;
            }

            for (void* const one : held) what.give(one);
        }

        best.add(clock_type::now() - started);
    }

    return best.per_op(ops);
}

}  // namespace

int main() {
    constexpr unsigned ops = 4096;
    constexpr unsigned rounds = 5;
    constexpr unsigned touch_ops = 512;
    constexpr unsigned touch_rounds = 3;

    std::printf("1 MiB page, ns per take+give (best of %u rounds)\n\n", rounds);
    std::printf("  %-14s %14s %14s %14s\n", "", "take+give", "16 held", "take+write+give");

    for (const allocator& what : allocators) {
        const double bare = take_and_give(what, ops, rounds);
        const double cold = sixteen_at_once(what, ops, rounds);
        const double written = take_write_give(what, touch_ops, touch_rounds);

        std::printf("  %-14s %11.0f ns %11.0f ns %11.0f ns\n", what.name, bare, cold, written);
    }

    std::printf("\n  %zu allocations, checksum %zu\n", operations, checksum);

    return 0;
}
