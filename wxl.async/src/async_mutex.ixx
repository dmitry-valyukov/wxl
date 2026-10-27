module;

#include "coroutine_checks.h"

export module wxl.async:async_mutex;

import :async_semaphore;
import :cancellation;
import :coroutine_checks;
import wxl.core;
import std;

export namespace wxl::async {

/// A mutex for the coroutines of one thread, locked without blocking it: `auto held = co_await
/// mutex.lock(token);` suspends the coroutine until the mutex is free, and the guard it gives
/// unlocks as it goes. A semaphore of one permit (async_semaphore) with the names of a mutex and
/// the guard over it, and nothing of its own beyond that: the waiters stand in the semaphore's
/// line, first come, first served; the next holder is resumed on the loop's next turn rather
/// than inside the unlock; and a wait ends early by its token.
///
/// **Not recursive.** A coroutine locking a mutex it holds waits for itself, for ever -- under
/// a token, until the token ends the wait. On one thread nothing can tell cheaply which
/// coroutine holds it, and it is the deadlock std::mutex has too.
///
/// One thread, the coroutines'; a build that checks coroutines stops at a call from any other,
/// and at a mutex destroyed while it is locked or somebody waits for it. Neither copied nor
/// moved, as the semaphore in it is not -- which is also why it does not derive from
/// core::noncopyable: a second such base at the address of the semaphore's own would cost a
/// word of padding.
class async_mutex
{
public:
    class guard;
    class lock_awaiter;

    /// Unlocked.
    inline async_mutex() noexcept : semaphore_(1, 1) {}

    /// Unlocked, with nobody waiting: a guard that outlived its mutex would unlock into one that
    /// is gone, and a build that checks stops here rather than let it. Where a destructor is
    /// called nothing gives the line, so the check names its own.
    inline ~async_mutex() {
        coro_check(semaphore_.count_ == 1, "async_mutex: destroyed while locked",
                   detail::site::current());
    }

    /// Locks it if it is free, and never suspends. While anybody waits for it, it is not free.
    /// \return a guard that owns the lock, or one that owns nothing.
    /// \param where the caller's line, for a strict build's report; left to its default.
    [[nodiscard]] inline guard try_lock(detail::site where = detail::site::current()) noexcept;

    /// The wait for the lock, to co_await where it is made; it gives the guard that owns it.
    /// Free, it is locked without suspending; otherwise the coroutine stands in line until an
    /// unlock hands the lock over, and is resumed on the loop's next turn. Under `token` the wait
    /// ends early with operation_canceled_exception, the lock untaken.
    ///
    /// \param token what ends the wait, required: the holder may keep the lock as long as it
    ///        pleases. A token of nobody is accepted and costs nothing.
    /// \param where the caller's line, for a strict build's report; left to its default.
    [[nodiscard]] inline lock_awaiter lock(cancellation_token token,
                                           detail::site where = detail::site::current());

private:
    async_semaphore semaphore_;
};

/// The lock held: what lock() and try_lock() give, and what unlocks it -- by unlock(), or by
/// the destructor if unlock() has not been called. Moved, the lock goes with it, and the guard
/// moved from owns nothing; never copied.
///
/// Unlocking hands the lock to the first coroutine in line, if any, and resumes nobody here: the
/// code after the unlock -- the rest of the holder's scope, the destructors after this one --
/// runs to its end before the next holder does.
class [[nodiscard("a guard dropped at once unlocks at once")]] async_mutex::guard
{
public:
    /// Takes over the lock `other` owns, if any; `other` owns nothing after.
    inline guard(guard&& other) noexcept : mutex_(std::exchange(other.mutex_, nullptr)) {}

    /// Takes over the lock `other` owns, if any, and unlocks the one this guard owned. Through a
    /// local, so that a guard moved onto itself stays as it is.
    inline guard& operator=(guard&& other) noexcept {
        guard taken(std::move(other));
        std::swap(mutex_, taken.mutex_);
        return *this;
    }

    /// Unlocks, if the guard still owns the lock. Where a destructor is called nothing gives
    /// the line, so a check here names its own.
    inline ~guard() {
        if (mutex_) mutex_->semaphore_.release(1, detail::site::current());
    }

    /// Unlocks now, before the guard goes; the destructor then does nothing more. Only a guard
    /// that owns the lock may: a build that checks stops at one that does not -- unlocked
    /// already, moved from, or a try_lock() that failed.
    /// \param where the caller's line, for a strict build's report; left to its default.
    inline void unlock(detail::site where = detail::site::current()) noexcept {
        coro_check(mutex_, "async_mutex::guard: unlocked without owning the lock", where);
        std::exchange(mutex_, nullptr)->semaphore_.release(1, where);
    }

    /// Whether the guard owns the lock.
    inline bool owns_lock() const noexcept { return mutex_ != nullptr; }

    /// owns_lock().
    inline explicit operator bool() const noexcept { return owns_lock(); }

private:
    friend class async_mutex;
    friend class async_mutex::lock_awaiter;

    /// Owns the lock of `mutex`, or nothing for null.
    inline explicit guard(async_mutex* mutex) noexcept : mutex_(mutex) {}

    /// The mutex whose lock the guard owns; null when it owns none.
    async_mutex* mutex_;
};

/// What `co_await mutex.lock(token)` waits in: the semaphore's wait for its one permit
/// (async_semaphore::acquire_awaiter), giving the guard when it is over. Made in place in the
/// awaiting frame, and neither copied nor moved, as the wait in it is not.
class [[nodiscard("a lock takes nothing until it is co_awaited")]] async_mutex::lock_awaiter
{
public:
    lock_awaiter(const lock_awaiter&) = delete;
    lock_awaiter& operator=(const lock_awaiter&) = delete;

    // Each hook takes the place of the co_await and hands it on to the semaphore's, so that a
    // strict build reports the line of the co_await rather than this file's.

    /// \param where the line of the co_await, for a strict build's report.
    inline bool await_ready(detail::site where = detail::site::current()) {
        return acquire_.await_ready(where);
    }

    /// \param awaiting the coroutine that waits.
    /// \param where the line of the co_await, for a strict build's report.
    template <class Promise>
    inline void await_suspend(std::coroutine_handle<Promise> awaiting,
                              detail::site where = detail::site::current()) {
        acquire_.await_suspend(awaiting, where);
    }

    /// \return the guard that owns the lock.
    /// \throw operation_canceled_exception if the token ended the wait.
    /// \param where the line of the co_await, for a strict build's report.
    inline guard await_resume(detail::site where = detail::site::current()) {
        acquire_.await_resume(where);
        return guard(&mutex_);
    }

private:
    friend class async_mutex;

    inline lock_awaiter(async_mutex& mutex, cancellation_token token, detail::site where)
        : mutex_(mutex), acquire_(mutex.semaphore_.acquire(std::move(token), where)) {}

    async_mutex& mutex_;
    async_semaphore::acquire_awaiter acquire_;
};

inline async_mutex::guard async_mutex::try_lock(detail::site where) noexcept {
    return guard(semaphore_.try_acquire(where) ? this : nullptr);
}

inline async_mutex::lock_awaiter async_mutex::lock(cancellation_token token, detail::site where) {
    return lock_awaiter(*this, std::move(token), where);
}

}  // export namespace wxl::async
