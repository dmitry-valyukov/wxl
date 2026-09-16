module;

#include "abi.h"

export module wxl.core:lightweight_semaphore;

import :noncopyable;
import :semaphore;
import :time;
import std;

export namespace wxl::core {

/// Lightweight semaphore (Preshing's pattern): acquire()/try_acquire() only touch the
/// underlying OS semaphore when the userspace counter has no free permit to hand out;
/// release() only wakes the OS semaphore when there is an actual waiter to wake.
///
/// A negative counter_ means its magnitude is the number of waiters owed a wakeup.
class lightweight_semaphore : public noncopyable
{
public:
    explicit lightweight_semaphore(int32_t initial_count)
        : counter_(initial_count)
        , sema_(0)
    {
        assert(initial_count >= 0);
    }

    void acquire() {
        if(counter_.fetch_sub(1, std::memory_order_acquire) > 0)
            return;

        sema_.acquire();
    }

    /// Blocks until a permit is acquired or \p timeout elapses. Returns false only on timeout.
    bool try_acquire_for(duration timeout) {
        if(counter_.fetch_sub(1, std::memory_order_acquire) > 0)
            return true;

        return acquire_slow(timeout);
    }

    bool try_acquire() {
        int32_t counter = counter_.load(std::memory_order_relaxed);

        while(counter > 0) {
            if(counter_.compare_exchange_weak(counter, counter - 1,
                                               std::memory_order_acquire, std::memory_order_relaxed))
                return true;
        }

        return false;
    }

    void release() {
        if(counter_.fetch_add(1, std::memory_order_release) < 0)
            sema_.release();
    }

private:
    bool acquire_slow(duration timeout);

    std::atomic<int32_t> counter_;
    semaphore sema_;
};

}
