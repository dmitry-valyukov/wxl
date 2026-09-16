#include <gtest/gtest.h>

import std;
import wxl.core;

using namespace wxl::core;

namespace {

// The clock is never asked anything here. The timer is told the moment it starts from, and
// every answer is checked against a `now` made up on the spot -- which is the whole reason
// the two-argument constructor exists.
constexpr time_stamp start = time_stamp::from_ticks(1'000'000);

TEST(TimeoutTimerTest, CountsDownFromTheStartItWasGiven) {
    const timeout_timer timer(start, duration::from_ms(100));

    EXPECT_EQ(timer.remaining(start), duration::from_ms(100));
    EXPECT_EQ(timer.remaining(start + time_span::from_ms(40)), duration::from_ms(60));
    EXPECT_EQ(timer.remaining(start + time_span::from_ms(99)), duration::from_ms(1));
}

TEST(TimeoutTimerTest, IsSpentAtTheDeadlineAndStaysThere) {
    const timeout_timer timer(start, duration::from_ms(100));

    EXPECT_EQ(timer.remaining(start + time_span::from_ms(100)), duration::zero());
    EXPECT_EQ(timer.remaining(start + time_span::from_sec(1000)), duration::zero());

    // What every caller actually asks it -- `while (timer.remaining())` -- and the reason
    // remaining() saturates at zero instead of going negative.
    EXPECT_FALSE(timer.remaining(start + time_span::from_ms(100)));
}

TEST(TimeoutTimerTest, AZeroTimeoutIsSpentBeforeItStarts) {
    // A caller that asked not to wait at all: the loop over remaining() runs no rounds.
    const timeout_timer timer(start, duration::zero());

    EXPECT_FALSE(timer.remaining(start));
}

TEST(TimeoutTimerTest, AMomentBeforeTheStartHasMoreLeftThanTheWholeTimeout) {
    // remaining() does not require its argument to come after the start: it answers about
    // the deadline, and the answer stays a duration -- non-negative -- either way.
    const timeout_timer timer(start, duration::from_ms(100));

    EXPECT_EQ(timer.remaining(start - time_span::from_ms(10)), duration::from_ms(110));
}

}  // namespace
