module;
#include "pch.h"

module wxl.core;
import std;

namespace wxl::core {

namespace {

/// Returns the mutex's semaphore, lazily creating it on first contention.
HANDLE ensure_semaphore(std::atomic<HANDLE>& slot) {
    if (HANDLE sem = slot.load(std::memory_order_acquire)) return sem;

    HANDLE created = ::CreateSemaphoreW(nullptr, 0, 1, nullptr);

    if (created == nullptr) [[unlikely]]
        abort("Failed to create mutex semaphore");

    HANDLE expected = nullptr;

    if (slot.compare_exchange_strong(expected, created, std::memory_order_acq_rel,
                                     std::memory_order_acquire))
        return created;

    ::CloseHandle(created);
    return expected;
}

constexpr int spin_iterations = 64;

/// Retries try_acquire() until it succeeds (and until the deadline, for the second form).
/// Spins with a "pause" hint for a bounded number of attempts first -- most contended
/// critical sections are held for only a handful of instructions, so this often succeeds
/// well before it would be worth paying for a kernel wait/wake round-trip -- then falls
/// back to parking on sem.
///
/// Two functions rather than one taking "forever" as a value: waiting without a deadline
/// keeps no timer, asks for no remaining time and hands the OS its own INFINITE.
///@{
template <typename TryAcquire>
void wait_for_mutex(HANDLE sem, TryAcquire try_acquire) {
    for (int i = 0; i < spin_iterations; ++i) {
        if (try_acquire()) [[likely]]
            return;

        _mm_pause();
    }

    while (!try_acquire()) ::WaitForSingleObject(sem, INFINITE);
}

template <typename TryAcquire>
bool wait_for_mutex(HANDLE sem, duration timeout, TryAcquire try_acquire) {
    const timeout_timer timer(timeout);

    for (int i = 0; i < spin_iterations; ++i) {
        if (try_acquire()) [[likely]]
            return true;

        if (!timer.remaining()) return try_acquire();

        _mm_pause();
    }

    while (true) {
        if (try_acquire()) [[likely]]
            return true;

        const duration remaining = timer.remaining();

        if (!remaining) return try_acquire();

        ::WaitForSingleObject(sem, to_os_timeout_ms(remaining));
    }
}
///@}

}  // namespace

mutex::~mutex() {
    assert(th_id_.load(std::memory_order_relaxed) == 0);

    if (HANDLE sem = sem_.load(std::memory_order_relaxed)) ::CloseHandle(sem);
}

void mutex::acquire_slow() {
    wait_for_mutex(ensure_semaphore(sem_), [this] { return try_acquire(); });
}

bool mutex::acquire_slow(duration timeout) {
    return wait_for_mutex(ensure_semaphore(sem_), timeout, [this] { return try_acquire(); });
}

recursive_mutex::~recursive_mutex() {
    assert(th_id_.load(std::memory_order_relaxed) == 0);

    if (HANDLE sem = sem_.load(std::memory_order_relaxed)) ::CloseHandle(sem);
}

void recursive_mutex::acquire_slow() {
    wait_for_mutex(ensure_semaphore(sem_), [this] { return try_acquire(); });
}

bool recursive_mutex::acquire_slow(duration timeout) {
    return wait_for_mutex(ensure_semaphore(sem_), timeout, [this] { return try_acquire(); });
}

}  // namespace wxl::core
