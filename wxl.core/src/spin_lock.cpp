module;
#include "pch.h"

module wxl.core;
import std;

using namespace wxl::core;

bool spin_lock::try_lock_impl(size_t spin_count) noexcept {
    ssize_t expected = 0;

    for (size_t i = 0; i < spin_count; i++) {
        if (target_.compare_exchange_strong(expected, 1, std::memory_order_acquire,
                                            std::memory_order_relaxed))
            return true;

        expected = 0;
        cpu_pause();
    }

    return false;
}

void spin_lock::lock_impl() noexcept {
    ssize_t expected = 0;

    while (!target_.compare_exchange_weak(expected, 1, std::memory_order_acquire,
                                          std::memory_order_relaxed)) {
        expected = 0;
        cpu_pause();
    }
}

void spin_lock::unlock_impl() noexcept { target_.store(0, std::memory_order_release); }
