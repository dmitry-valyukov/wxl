module;

#include "abi.h"

export module wxl.core:thread_guard;

import :checks;
import :noncopyable;
import :thread_id;
import std;

export namespace wxl::core {

/// Binds a Tag to the thread that constructed the guard, so the per-thread
/// statics carrying that Tag can assert they are never touched from another
/// one.
///
/// The identity is the OS thread id read straight off the TEB rather than a
/// std::thread::id: the check sits on allocation paths, where a call into the
/// CRT would cost more than the comparison it guards.
template <typename Tag>
class thread_guard : public noncopyable
{
    // 0 is never a live thread id, so it doubles as "no thread claimed yet".
    static constinit inline thread_id s_thread_id = 0;

public:
    thread_guard() {
        if (s_thread_id != 0) [[unlikely]]
            abort("thread_guard: already initialized");

        s_thread_id = current_thread_id();
    }

    ~thread_guard() { s_thread_id = 0; }

    /// The guard is claimed by the calling thread. False before construction
    /// and after destruction as well, not only on a foreign thread.
    static bool is_safe() noexcept { return s_thread_id == current_thread_id(); }

    static void debug_check_thread() noexcept {
        assert(is_safe() && "thread_guard: thread mismatch");
    }
};

}  // export namespace wxl::core
