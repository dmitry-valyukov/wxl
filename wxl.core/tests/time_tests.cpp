#include <gtest/gtest.h>

import std;
import wxl.core;

using namespace wxl::core;

// The two bridges between a duration and the "milliseconds, and negative means forever"
// convention the OS wait calls speak. Worth pinning down precisely, because that boundary is
// the only place a mistake would show -- and because it is where the two kinds of waiting
// part ways: what comes back from timeout_from_ms() is either a deadline or nothing at all.

TEST(DurationTest, FromTimeoutMsHasNothingToGiveWhenTheNumberSaysForever) {
    EXPECT_FALSE(timeout_from_ms(-1).has_value());
    EXPECT_FALSE(timeout_from_ms(-1000).has_value());

    EXPECT_EQ(timeout_from_ms(0), duration::zero());
    EXPECT_EQ(timeout_from_ms(250), duration::from_ms(250));
}

TEST(DurationTest, ToOsTimeoutMsIsPlainMilliseconds) {
    EXPECT_EQ(to_os_timeout_ms(duration::zero()), 0u);
    EXPECT_EQ(to_os_timeout_ms(duration::from_ms(1500)), 1500u);
    EXPECT_EQ(to_os_timeout_ms(duration::from_sec(2)), 2000u);
}

TEST(DurationTest, ATimeoutTooLargeToExpressStopsShortOfForever) {
    // 0xFFFFFFFF is Win32's INFINITE, and it is the one value this conversion must never
    // produce: turning a long wait into an endless one is the rounding error a caller could
    // never recover from. Waiting forever is a call of its own and never comes through here.
    const duration a_century = duration::from_sec(100 * 365 * 24 * 60 * 60);

    EXPECT_EQ(to_os_timeout_ms(a_century), 0xFFFF'FFFEu);
}

// time_span arithmetic. Scaling goes through long double, so it rounds -- and on round
// values it has to round to exactly the value asked for. The conversions to and from
// std::chrono are pinned here too, since they are the only way a span crosses into
// standard-library code.

TEST(TimeSpanTest, AddsAndSubtractsSpans) {
    time_span t = time_span::from_sec(42);

    t += time_span::from_sec(42);
    EXPECT_EQ(time_span::from_sec(84), t);

    t -= time_span::from_sec(42);
    EXPECT_EQ(time_span::from_sec(42), t);

    EXPECT_EQ(time_span::from_sec(84), time_span::from_sec(42) + time_span::from_sec(42));
    EXPECT_EQ(time_span::from_sec(0), time_span::from_sec(42) - time_span::from_sec(42));
}

TEST(TimeSpanTest, ScalesByANumber) {
    EXPECT_EQ(time_span::from_sec(6), time_span::from_sec(42) / 7);
    EXPECT_EQ(time_span::from_sec(42), time_span::from_sec(6) * 7);

    time_span t = time_span::from_sec(84);

    t /= 14;
    EXPECT_EQ(time_span::from_sec(6), t);

    t *= 7;
    EXPECT_EQ(time_span::from_sec(42), t);
}

TEST(TimeSpanTest, NegatesAndTakesAbsoluteValue) {
    const time_span t = time_span::from_ms(250);

    EXPECT_EQ(-250, (-t).total_milliseconds());
    EXPECT_EQ(t, (-t).abs());
    EXPECT_EQ(t, t.abs());

    EXPECT_FALSE(static_cast<bool>(time_span()));
    EXPECT_TRUE(static_cast<bool>(t));
}

TEST(TimeSpanTest, ComesFromAndGoesToMicroseconds) {
    // A microsecond count divisible by neither a second nor a millisecond: the case where
    // a lost remainder would show.
    const int64_t microseconds = 123'456'789;

    EXPECT_EQ(microseconds, time_span::from_us(microseconds).total_microseconds());
    EXPECT_EQ(microseconds / 1000, time_span::from_us(microseconds).total_milliseconds());
    EXPECT_EQ(microseconds / 1'000'000, time_span::from_us(microseconds).total_seconds());

    const time_span from_chrono = std::chrono::microseconds(microseconds);
    EXPECT_EQ(microseconds, from_chrono.total_microseconds());
}

TEST(TimeSpanTest, ADurationWidensToASpan) {
    const duration d = duration::from_ms(1500);
    const time_span t = d;

    EXPECT_EQ(1500, t.total_milliseconds());
    EXPECT_EQ(time_span::from_ms(1500), t);
}
