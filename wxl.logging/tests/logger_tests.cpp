#include <gtest/gtest.h>

#include <fmt/format.h>

#include <chrono>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

import wxl.logging;

#include "recording_output.h"

/// A value that counts how often it is actually formatted, which is how the
/// tests tell "the message was skipped" from "the message was written and
/// thrown away".
struct counted_value {
    static inline int formats = 0;
};

template <>
struct fmt::formatter<counted_value> : fmt::formatter<std::string_view> {
    auto format(const counted_value&, fmt::format_context& context) const {
        ++counted_value::formats;

        return fmt::formatter<std::string_view>::format("counted", context);
    }
};

using wxl::logging::severity;

namespace {

/// The prefix every line starts with, as the tests read it back.
bool looks_like_a_timestamp(const std::string_view line) {
    if (line.size() < wxl::logging::timestamp_size) return false;

    for (std::size_t at = 0; at < wxl::logging::timestamp_size; ++at) {
        const char character = line[at];
        const bool punctuation = (at == 8 && character == '-') ||
                                 ((at == 11 || at == 14) && character == ':') ||
                                 (at == 17 && character == '.');

        if (!punctuation && (character < '0' || character > '9')) return false;
    }

    return true;
}

TEST(logger, a_line_is_the_message_and_a_newline) {
    recording_output kept;
    wxl::logging::logger log(kept);

    log.info("hello");

    ASSERT_EQ(kept.size(), 1u);
    EXPECT_EQ(kept.messages().front(), "hello\n");
    EXPECT_TRUE(kept.lines().front().ends_with("hello\n"));
}

TEST(logger, the_arguments_are_formatted_into_the_message) {
    recording_output kept;
    wxl::logging::logger log(kept);

    log.info("listening on {}:{}", "127.0.0.1", 8080);
    log.info("{:.2f}% of {} done", 12.5, 40);

    const auto messages = kept.messages();

    ASSERT_EQ(messages.size(), 2u);
    EXPECT_EQ(messages[0], "listening on 127.0.0.1:8080\n");
    EXPECT_EQ(messages[1], "12.50% of 40 done\n");
}

TEST(logger, every_line_starts_with_a_timestamp_of_a_fixed_width) {
    recording_output kept;
    wxl::logging::logger log(kept);

    log.info("x");

    EXPECT_TRUE(looks_like_a_timestamp(kept.lines().front()));
}

TEST(logger, the_time_in_the_entry_is_when_the_message_was_made) {
    recording_output kept;
    wxl::logging::logger log(kept);

    const auto before = wxl::logging::log_clock::now();
    log.info("x");
    const auto after = wxl::logging::log_clock::now();

    const auto stamped = kept.times().front();

    EXPECT_LE(before, stamped);
    EXPECT_LE(stamped, after);
}

TEST(logger, an_ordinary_message_goes_unmarked_and_the_rest_name_their_level) {
    recording_output kept;
    wxl::logging::logger log(kept, severity::debug);

    log.info("ordinary");
    log.warn("odd");
    log.error("bad");
    log.fatal("over");
    log.trace("where");
    log.debug("what");

    const auto lines = kept.lines();

    ASSERT_EQ(lines.size(), 6u);
    EXPECT_EQ(lines[0].find('<'), std::string::npos);
    EXPECT_NE(lines[1].find("<WARN>"), std::string::npos);
    EXPECT_NE(lines[2].find("<ERROR>"), std::string::npos);
    EXPECT_NE(lines[3].find("<FATAL>"), std::string::npos);
    EXPECT_NE(lines[4].find("<TRACE>"), std::string::npos);
    EXPECT_NE(lines[5].find("<DEBUG>"), std::string::npos);
}

TEST(logger, the_entry_knows_where_its_prefix_ends) {
    recording_output kept;
    wxl::logging::logger log(kept);

    log.warn("the message itself");

    const std::string line = kept.lines().front();
    const std::string message = kept.messages().front();

    EXPECT_EQ(message, "the message itself\n");
    EXPECT_TRUE(line.ends_with(message));
    EXPECT_EQ(line.substr(0, line.size() - message.size()).back(), ' ');
}

TEST(logger, a_message_below_the_level_is_not_written) {
    recording_output kept;
    wxl::logging::logger log(kept, severity::warn);

    log.fatal("kept");
    log.error("kept");
    log.warn("kept");
    log.info("dropped");
    log.trace("dropped");
    log.debug("dropped");

    EXPECT_EQ(kept.size(), 3u);
}

TEST(logger, nothing_is_formatted_for_a_message_that_is_not_written) {
    recording_output kept;
    wxl::logging::logger log(kept, severity::info);

    // The argument is evaluated -- the language gives no way around that
    // without macros -- but its formatter must not run, and the formatter is
    // where the work of a log message actually is.
    counted_value::formats = 0;

    log.debug("{}", counted_value{});

    EXPECT_EQ(kept.size(), 0u);
    EXPECT_EQ(counted_value::formats, 0);

    log.info("{}", counted_value{});

    EXPECT_EQ(kept.size(), 1u);
    EXPECT_EQ(counted_value::formats, 1);
}

TEST(logger, the_level_can_be_moved_while_it_runs) {
    recording_output kept;
    wxl::logging::logger log(kept, severity::info);

    log.debug("dropped");
    EXPECT_EQ(kept.size(), 0u);

    log.set_level(severity::debug);
    EXPECT_EQ(log.level(), severity::debug);

    log.debug("kept");
    EXPECT_EQ(kept.size(), 1u);
}

TEST(logger, one_entry_serves_every_line) {
    recording_output kept;
    wxl::logging::logger log(kept);

    log.info("a long first line that leaves plenty of room behind it");
    log.info("short");

    EXPECT_EQ(kept.messages()[1], "short\n");
}

TEST(logger, the_level_can_be_a_template_argument) {
    recording_output kept;
    wxl::logging::logger log(kept, severity::debug);

    log.log<severity::error>("generic {}", 1);

    ASSERT_EQ(kept.size(), 1u);
    EXPECT_EQ(kept.levels().front(), severity::error);
    EXPECT_EQ(kept.messages().front(), "generic 1\n");
}

TEST(logger, the_null_output_swallows_everything) {
    wxl::logging::logger log(wxl::logging::null_output::instance(), severity::debug);

    log.info("nobody is listening");
    log.fatal("still nobody");

    SUCCEED();
}

}  // namespace
