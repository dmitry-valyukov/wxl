module;

#include <intrin.h>

export module wxl.async:thread;

import wxl.core;
import std;

export namespace wxl::async {

/// Everything one can ask of, or do to, the thread one is already running on: identity,
/// waiting, spinning, affinity and priority. Threads themselves are started through
/// thread_group, which owns their lifetime and hands back a future for the body.
class thread
{
public:
    static core::thread_id current_thread_id() noexcept { return core::current_thread_id(); }

    static core::thread_id self() noexcept { return core::current_thread_id(); }

    /// Index of the processor the calling thread is currently running on.
    static int current_processor_number() noexcept;

    /// Suspends the calling thread for the given number of milliseconds.
    static void sleep(size_t milliseconds);

    static void sleep(core::duration timeout) {
        sleep(static_cast<size_t>(timeout.total_milliseconds()));
    }

    /// Busy-waits (spinning, not sleeping) for approximately the given number of microseconds.
    static void spin_wait_micro(size_t usec);

    /// Busy-waits (spinning, not sleeping) for approximately the given number of nanoseconds.
    static void spin_wait_nano(size_t nanosec);

    /// Executes a "pause" instruction the given number of times.
    static void spin_wait(int iterations) noexcept;

    /// Relinquishes the remainder of the calling thread's time slice.
    /// \return true if execution was actually switched to another thread.
    static bool yield() noexcept;

    /// Logical processors a thread is allowed to run on (the first has index 0).
    using cpu_indexes = std::set<size_t>;

    static void set_affinity(const cpu_indexes& indexes);
    static void set_affinity(size_t cpu_index);

    static void priority(int value);
    static int priority();

    static constexpr size_t max_thread_name_len = 32;
};

inline void thread::spin_wait(int iterations) noexcept {
    while (iterations-- > 0) _mm_pause();
}

inline void thread::spin_wait_micro(size_t usec) { spin_wait_nano(usec * 1000); }

inline void thread::spin_wait_nano(size_t nanosec) {
    const core::time_stamp start = core::time_stamp::now();
    const core::time_span budget = core::time_span::from_ticks(static_cast<int64_t>((nanosec + 99) / 100));

    bool first_iteration = true;

    while (core::time_stamp::now() - start < budget) {
        spin_wait(1);

        if (first_iteration) {
            first_iteration = false;

            // If even one pause already overshot the budget, spinning further would
            // only overshoot more; stop now rather than looping needlessly.
            if (core::time_stamp::now() - start >= budget) return;
        }
    }
}

}  // export namespace wxl::async
