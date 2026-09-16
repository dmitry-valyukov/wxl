#include <gtest/gtest.h>

#include <mutex>
#include <string>
#include <string_view>
#include <vector>

import wxl.logging;

#include "recording_output.h"

using wxl::logging::facility;
using wxl::logging::severity;

namespace {

TEST(facility, a_root_is_its_own_full_name) {
    const facility app("App");

    EXPECT_EQ(app.name(), "App");
    EXPECT_EQ(app.full_name(), "App");
    EXPECT_EQ(app.parent(), nullptr);
}

TEST(facility, the_full_name_is_the_whole_chain) {
    const facility app("App");
    const facility storage("Storage", &app);
    const facility watcher("Watcher", &storage);

    EXPECT_EQ(watcher.full_name(), "App.Storage.Watcher");
    EXPECT_EQ(watcher.name(), "Watcher");
    EXPECT_EQ(storage.full_name(), "App.Storage");
    EXPECT_EQ(watcher.parent(), &storage);
}

TEST(facility, the_own_name_is_a_view_into_the_full_one) {
    const facility app("App");
    const facility part("Part", &app);

    EXPECT_EQ(part.name().data(), part.full_name().data() + part.full_name().size() - 4);
}

TEST(facility, a_name_with_a_dot_in_it_is_kept_as_given) {
    const facility odd("first.second");

    EXPECT_EQ(odd.name(), "first.second");
    EXPECT_EQ(odd.full_name(), "first.second");
}

TEST(facility, the_level_can_be_moved_while_it_runs) {
    facility watcher("Watcher", nullptr, severity::info);

    EXPECT_TRUE(watcher.enabled(severity::warn));
    EXPECT_FALSE(watcher.enabled(severity::debug));

    watcher.set_level(severity::debug);

    EXPECT_TRUE(watcher.enabled(severity::debug));
}

TEST(facility, the_name_goes_into_the_line_in_brackets) {
    recording_output kept;
    wxl::logging::logger log(kept, severity::debug);

    // A facility of its own defaults to info, so one that is to be traced has
    // to say so -- that is the whole point of it having a level.
    const facility app("App", nullptr, severity::debug);
    const facility watcher("Watcher", &app, severity::debug);

    log.trace(watcher, "stopped");

    const std::string line = kept.lines().front();

    EXPECT_NE(line.find("<TRACE> [App.Watcher] "), std::string::npos);
    EXPECT_EQ(kept.messages().front(), "stopped\n");
}

TEST(facility, both_filters_are_in_series_and_the_quieter_one_wins) {
    recording_output kept;
    wxl::logging::logger log(kept, severity::debug);

    facility quiet("Quiet", nullptr, severity::warn);

    log.warn(quiet, "kept");
    log.debug(quiet, "the facility is too quiet for this");

    EXPECT_EQ(kept.size(), 1u);

    // ...and the other way round: a facility turned all the way up cannot make
    // a logger say more than it was told to.
    log.set_level(severity::error);
    quiet.set_level(severity::debug);

    log.warn(quiet, "the logger is too quiet for this");

    EXPECT_EQ(kept.size(), 1u);
}

TEST(facility, one_turned_up_says_more_than_the_others) {
    recording_output kept;
    wxl::logging::logger log(kept, severity::debug);

    const facility ordinary("Ordinary", nullptr, severity::info);
    const facility watched("Watched", nullptr, severity::debug);

    log.debug(ordinary, "not this one");
    log.debug(watched, "but this one");

    ASSERT_EQ(kept.size(), 1u);
    EXPECT_EQ(kept.messages().front(), "but this one\n");
}

}  // namespace
