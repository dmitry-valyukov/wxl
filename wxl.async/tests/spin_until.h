#pragma once

import std;
import wxl.core;
import wxl.async;

namespace wxl::async {

/// Busy-waits until the predicate holds, pausing between attempts.
///
/// A test-only helper, and deliberately not part of the library: production code never
/// polls a flag in a loop, it blocks on a signal and is woken when the thing it waits for
/// changes. Tests are where a predicate legitimately has nothing to be woken by.
///
/// The companion in test-support, become_true(), yields between attempts instead of
/// pausing; prefer it for waits long enough for the scheduler to matter.
template <class unary_predicate>
inline void spin_until(unary_predicate p) {
    while (!p()) thread::spin_wait(1);
}

/// Busy-waits until the predicate holds or the timeout elapses.
/// \return false if timed out.
template <class unary_predicate>
inline bool spin_until(unary_predicate p, core::duration t) {
    const core::timeout_timer timer(t);

    while (!p()) {
        if (!timer.remaining()) return false;

        thread::spin_wait(1);
    }

    return true;
}

}  // namespace wxl::async
