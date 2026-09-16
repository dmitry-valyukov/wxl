module;

#include "platform.h"

export module wxl.core:semaphore;

import :noncopyable;
import :time;
import std;

export namespace wxl::core {

/// RAII wrapper over a Windows semaphore object
/// (CreateSemaphore/ReleaseSemaphore/WaitForSingleObject).
class semaphore : public noncopyable
{
    HANDLE handle_;

public:
    /// Creates a semaphore with the given initial count; max_count bounds how
    /// high release() can drive it (defaults to a value effectively unbounded
    /// for this class's use cases).
    explicit semaphore(long initial_count, long max_count = 0x7fffffffL);

    ~semaphore() { ::CloseHandle(handle_); }

    /// Increments the count by one, waking a single waiter if any.
    void release() { ::ReleaseSemaphore(handle_, 1, nullptr); }

    /// Blocks until the semaphore is signaled.
    void acquire() { ::WaitForSingleObject(handle_, INFINITE); }

    /// Blocks until the semaphore is signaled or \p timeout elapses. Returns false only on
    /// timeout. Named after std::counting_semaphore's, which does the same thing.
    bool try_acquire_for(duration timeout) {
        return ::WaitForSingleObject(handle_, to_os_timeout_ms(timeout)) == WAIT_OBJECT_0;
    }

    /// Non-blocking acquire: returns false immediately if not currently signaled.
    bool try_acquire() { return ::WaitForSingleObject(handle_, 0) == WAIT_OBJECT_0; }

    HANDLE handle() const noexcept { return handle_; }
};

}  // export namespace wxl::core
