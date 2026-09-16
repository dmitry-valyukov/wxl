module;
#include "pch.h"

module wxl.core;
import std;

namespace wxl::core {

bool lightweight_semaphore::acquire_slow(duration timeout) {
    if (sema_.try_acquire_for(timeout)) return true;

    int32_t old_value = counter_.load(std::memory_order_relaxed);

    while (true) {
        if (old_value < 0) {
            if (counter_.compare_exchange_weak(old_value, old_value + 1, std::memory_order_relaxed))
                return false;

            continue;
        }

        sema_.acquire();
        return true;
    }
}

}  // namespace wxl::core
