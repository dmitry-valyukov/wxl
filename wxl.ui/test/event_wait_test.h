#pragma once

// The state of the event waits as the tests see it. impl::event_wait keeps its lists and flags
// private; the tests read whether a turn of the queue is asked for, and put the flags back
// between tests, through this friend of the class.

#include "impl/event_waits.h"

namespace wxl::impl {

struct event_wait_test {
    /// Whether a turn that resumes the cancelled waits is in the queue already.
    static bool canceled_posted() noexcept { return event_wait::canceled_posted_; }

    static void set_canceled_posted(bool posted) noexcept { event_wait::canceled_posted_ = posted; }

    /// Back from going down, for the next test.
    static void set_closing(bool closing) noexcept { event_wait::closing_ = closing; }
};

}  // namespace wxl::impl
