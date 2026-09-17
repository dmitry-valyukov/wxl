export module wxl.core:timeout_timer;

import :time;

export namespace wxl::core {

/// Tracks a deadline (now + timeout) and reports the time remaining until it.
///
/// A deadline is all this is, and "no deadline" is not one of them: waiting without a
/// deadline is a call of its own everywhere in this library -- wait() beside
/// wait_for(timeout), acquire() beside try_acquire_for(timeout) -- and that call keeps no
/// timer, because it has nothing to count down to.
class timeout_timer
{
public:
    inline explicit timeout_timer(duration timeout) : timeout_timer(time_stamp::now(), timeout) {}

    /// Counts from \p start rather than from the current moment: the deadline is then one
    /// the caller already knows, which is also what makes the countdown checkable without
    /// asking the clock anything.
    inline timeout_timer(time_stamp start, duration timeout) : deadline_(start + timeout) {}

    inline duration remaining() const { return remaining(time_stamp::now()); }

    inline duration remaining(time_stamp now) const {
        if (now >= deadline_) return duration::zero();

        return duration(deadline_ - now);
    }

private:
    time_stamp deadline_;
};

}  // export namespace wxl::core
