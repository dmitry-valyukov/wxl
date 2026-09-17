module;

#include "platform.h"
#include "abi.h"

export module wxl.core:mutex;

import :noncopyable;
import :thread_id;
import :time;
import std;

export namespace wxl::core {

class mutex : public noncopyable
{
    std::atomic<HANDLE> sem_{};

    /// The owning thread, or 0. The ownership tests read it relaxed: only the owner ever
    /// writes its own id here, and its store of 0 on release is sequenced before whatever
    /// that thread reads next, so nobody can see its own id in a mutex it does not hold.
    /// The exchange that takes the mutex stays acquire -- that is what publishes the data
    /// the mutex guards.
    std::atomic<thread_id> th_id_{};

public:
    mutex() = default;

    ~mutex();

    /// True if the calling thread currently owns the mutex.
    inline bool is_synchronized() const {
        return th_id_.load(std::memory_order_relaxed) == current_thread_id();
    }

    /// Releases the mutex. Must be called by the owning thread only.
    inline void release() {
        assert(is_synchronized());

        th_id_.store(0, std::memory_order_release);

        if (HANDLE sem = sem_.load(std::memory_order_acquire)) ::ReleaseSemaphore(sem, 1, nullptr);
    }

    /// Blocks until the mutex is acquired.
    inline void acquire() {
        if (try_acquire()) [[likely]]
            return;
        acquire_slow();
    }

    /// Blocks until the mutex is acquired or \p timeout elapses. Returns false only on timeout.
    inline bool try_acquire_for(duration timeout) {
        if (try_acquire()) [[likely]]
            return true;
        return acquire_slow(timeout);
    }

    /// Non-blocking acquire: returns false immediately if not currently free.
    inline bool try_acquire() {
        thread_id expected = 0;
        return th_id_.compare_exchange_strong(expected, current_thread_id(),
                                              std::memory_order_acquire, std::memory_order_relaxed);
    }

private:
    void acquire_slow();
    bool acquire_slow(duration timeout);
};

class recursive_mutex : public noncopyable
{
    std::atomic<HANDLE> sem_{};

    /// The owning thread, or 0. The ownership tests read it relaxed: only the owner ever
    /// writes its own id here, so nobody can see its own id in a mutex it does not hold.
    std::atomic<thread_id> th_id_{};

    /// Plain, not atomic: only the owning thread ever touches it.
    uint32_t recursion_count_{};

public:
    recursive_mutex() = default;

    ~recursive_mutex();

    /// True if the calling thread currently owns the mutex.
    inline bool is_synchronized() const {
        return th_id_.load(std::memory_order_relaxed) == current_thread_id();
    }

    /// Releases one level of ownership. Must be called by the owning thread only.
    inline void release() {
        assert(is_synchronized());

        if (--recursion_count_ > 0) [[unlikely]]
            return;

        th_id_.store(0, std::memory_order_release);

        if (HANDLE sem = sem_.load(std::memory_order_acquire)) ::ReleaseSemaphore(sem, 1, nullptr);
    }

    /// Blocks until the mutex is acquired. Re-entrant: the owning thread may call this
    /// again without blocking, and must call release() the same number of times.
    inline void acquire() {
        if (try_acquire()) [[likely]]
            return;
        acquire_slow();
    }

    /// Blocks until the mutex is acquired or \p timeout elapses. Returns false only on timeout.
    /// Re-entrant: the owning thread may call this again without blocking, and must call
    /// release() the same number of times.
    inline bool try_acquire_for(duration timeout) {
        if (try_acquire()) [[likely]]
            return true;
        return acquire_slow(timeout);
    }

    /// Non-blocking acquire: returns false immediately if owned by another thread.
    inline bool try_acquire() {
        const thread_id self = current_thread_id();

        if (th_id_.load(std::memory_order_relaxed) == self) {
            ++recursion_count_;
            return true;
        }

        thread_id expected = 0;

        if (th_id_.compare_exchange_strong(expected, self, std::memory_order_acquire,
                                           std::memory_order_relaxed)) {
            recursion_count_ = 1;
            return true;
        }

        return false;
    }

private:
    void acquire_slow();
    bool acquire_slow(duration timeout);
};

}  // export namespace wxl::core
