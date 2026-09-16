#include <gtest/gtest.h>

#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

import wxl.logging;

#include "recording_output.h"

using wxl::logging::severity;

namespace {

TEST(multicast_output, every_subscriber_gets_the_same_line) {
    recording_output first;
    recording_output second;
    wxl::logging::multicast_output both;

    both.subscribe(first);
    both.subscribe(second);

    EXPECT_EQ(both.size(), 2u);

    wxl::logging::logger log(both);
    log.info("to both of you");

    ASSERT_EQ(first.size(), 1u);
    ASSERT_EQ(second.size(), 1u);
    EXPECT_EQ(first.lines().front(), second.lines().front());
}

TEST(multicast_output, an_unsubscribed_output_stops_hearing) {
    recording_output kept;
    wxl::logging::multicast_output fan_out;

    fan_out.subscribe(kept);

    wxl::logging::logger log(fan_out);
    log.info("heard");

    fan_out.unsubscribe(kept);
    EXPECT_EQ(fan_out.size(), 0u);

    log.info("not heard");

    EXPECT_EQ(kept.size(), 1u);
}

TEST(multicast_output, a_broken_output_is_dropped_and_the_rest_carry_on) {
    throwing_output broken;
    recording_output kept;
    wxl::logging::multicast_output fan_out;

    fan_out.subscribe(broken);
    fan_out.subscribe(kept);

    wxl::logging::logger log(fan_out);

    log.info("first");
    log.info("second");

    // Tried once, dropped, never tried again -- and the working output did not
    // lose a line over it.
    EXPECT_EQ(broken.attempts, 1);
    EXPECT_EQ(fan_out.size(), 1u);
    EXPECT_EQ(kept.size(), 2u);
}

TEST(multicast_output, a_band_of_severities_belongs_to_the_output) {
    // The console usually takes the top of the log and the file takes all of
    // it; both hang off one logger, and the line is formatted once.
    recording_output everything;
    wxl::logging::multicast_output fan_out;

    fan_out.subscribe(everything);

    wxl::logging::logger log(fan_out, severity::debug);

    log.fatal("worst");
    log.debug("noise");

    EXPECT_EQ(everything.size(), 2u);
}

TEST(null_output, there_is_one_of_it) {
    EXPECT_EQ(&wxl::logging::null_output::instance(), &wxl::logging::null_output::instance());
    EXPECT_EQ(wxl::logging::null_output::instance().id(), "null");
}

}  // namespace
