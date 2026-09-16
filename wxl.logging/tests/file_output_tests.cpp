#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

import wxl.logging;

#include "recording_output.h"

using wxl::logging::file_options;
using wxl::logging::file_output;
using wxl::logging::severity;

namespace {

/// Every test gets a directory of its own, named after itself, and leaves
/// nothing behind.
class file_output_test : public ::testing::Test
{
protected:
    void SetUp() override {
        directory_ = std::filesystem::temp_directory_path() / "wxl.logging.tests" /
                     ::testing::UnitTest::GetInstance()->current_test_info()->name();

        std::filesystem::remove_all(directory_);
        std::filesystem::create_directories(directory_);
    }

    void TearDown() override {
        std::error_code failed;
        std::filesystem::remove_all(directory_, failed);
    }

    std::filesystem::path log_path(std::string_view name = "app.log") const {
        return directory_ / name;
    }

    static std::string contents_of(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        std::ostringstream text;

        text << file.rdbuf();

        return text.str();
    }

    static void put(const std::filesystem::path& path, const std::string_view text) {
        std::ofstream file(path, std::ios::binary);

        file.write(text.data(), static_cast<std::streamsize>(text.size()));
    }

    std::filesystem::path directory_;
};

TEST_F(file_output_test, the_lines_are_in_the_file) {
    {
        file_output file(log_path());
        wxl::logging::logger log(file);

        log.info("first");
        log.warn("second");
    }

    const std::string written = contents_of(log_path());

    EXPECT_NE(written.find("first\n"), std::string::npos);
    EXPECT_NE(written.find("<WARN>"), std::string::npos);
    EXPECT_EQ(std::ranges::count(written, '\n'), 2);
}

TEST_F(file_output_test, a_line_is_readable_before_the_file_is_closed) {
    file_output file(log_path());
    wxl::logging::logger log(file);

    log.info("flushed as it goes");

    EXPECT_NE(contents_of(log_path()).find("flushed as it goes"), std::string::npos);
}

TEST_F(file_output_test, the_band_of_severities_belongs_to_the_file) {
    {
        file_output file(log_path(), {.shows = {.from = severity::fatal, .to = severity::info}});
        wxl::logging::logger log(file, severity::debug);

        log.info("kept");
        log.debug("dropped");
    }

    EXPECT_EQ(std::ranges::count(contents_of(log_path()), '\n'), 1);
}

TEST_F(file_output_test, a_full_file_is_followed_by_a_numbered_part) {
    {
        file_output file(log_path(), {.max_size = 64});
        wxl::logging::logger log(file);

        EXPECT_EQ(file.part_number(), 1u);

        // Each line is a good deal longer than the prefix alone, so a couple of
        // them are past 64 bytes for certain.
        log.info("a line long enough to fill a very small file all by itself");
        log.info("and the one after it");

        EXPECT_EQ(file.part_number(), 2u);
    }

    EXPECT_TRUE(std::filesystem::exists(log_path()));
    EXPECT_TRUE(std::filesystem::exists(log_path("app-part002.log")));
}

TEST_F(file_output_test, a_line_is_never_split_between_two_parts) {
    {
        file_output file(log_path(), {.max_size = 1});
        wxl::logging::logger log(file);

        log.info("whole");
        log.info("also whole");
    }

    EXPECT_TRUE(contents_of(log_path()).ends_with("whole\n"));
    EXPECT_TRUE(contents_of(log_path("app-part002.log")).ends_with("also whole\n"));
}

TEST_F(file_output_test, a_new_run_pushes_the_old_file_down_the_history) {
    put(log_path(), "the run before last\n");

    {
        file_output file(log_path(), {.rotate_by_run = true});
        wxl::logging::logger log(file);

        log.info("this run");
    }

    EXPECT_EQ(contents_of(log_path("app.log.1")), "the run before last\n");
    EXPECT_NE(contents_of(log_path()).find("this run"), std::string::npos);
}

TEST_F(file_output_test, the_history_only_goes_so_far_back) {
    put(log_path(), "newest");
    put(log_path("app.log.1"), "older");
    put(log_path("app.log.2"), "oldest");

    { file_output file(log_path(), {.rotate_by_run = true, .history_depth = 2}); }

    EXPECT_EQ(contents_of(log_path("app.log.1")), "newest");
    EXPECT_EQ(contents_of(log_path("app.log.2")), "older");
    EXPECT_FALSE(std::filesystem::exists(log_path("app.log.3")));
}

TEST_F(file_output_test, every_part_of_the_previous_run_moves_down) {
    put(log_path(), "part one");
    put(log_path("app-part002.log"), "part two");

    { file_output file(log_path(), {.rotate_by_run = true}); }

    EXPECT_EQ(contents_of(log_path("app.log.1")), "part one");
    EXPECT_EQ(contents_of(log_path("app-part002.log.1")), "part two");
}

TEST_F(file_output_test, appending_keeps_what_was_there) {
    put(log_path(), "from before\n");

    {
        file_output file(log_path(), {.append = true});
        wxl::logging::logger log(file);

        log.info("and now this");
    }

    const std::string written = contents_of(log_path());

    EXPECT_TRUE(written.starts_with("from before\n"));
    EXPECT_NE(written.find("and now this"), std::string::npos);
}

TEST_F(file_output_test, not_appending_starts_the_file_over) {
    put(log_path(), "from before\n");

    {
        file_output file(log_path());
        wxl::logging::logger log(file);

        log.info("only this");
    }

    EXPECT_FALSE(contents_of(log_path()).starts_with("from before"));
}

TEST_F(file_output_test, the_date_goes_into_the_name) {
    const auto today = std::chrono::floor<std::chrono::days>(wxl::logging::log_clock::now());
    const std::chrono::year_month_day date{today};

    char expected[32];
    std::snprintf(expected, sizeof expected, "app-%04d%02u%02u.log", static_cast<int>(date.year()),
                  static_cast<unsigned>(date.month()), static_cast<unsigned>(date.day()));

    { file_output file(log_path(), {.rotate_by_date = true}); }

    EXPECT_TRUE(std::filesystem::exists(log_path(expected)));
}

TEST_F(file_output_test, the_history_can_be_listed) {
    put(log_path(), "newest");
    put(log_path("app.log.1"), "older");

    const auto found = file_output::history_of(log_path(), 5);

    ASSERT_EQ(found.size(), 2u);
    EXPECT_EQ(found[0], log_path());
    EXPECT_EQ(found[1], log_path("app.log.1"));
}

/// A file that writes a banner at the top of every part it opens, which is what
/// the hook is there for.
class banner_file : public file_output
{
public:
    using file_output::file_output;

protected:
    void on_before_first_line() override { write_to_file("*** part ***\n"); }
};

TEST_F(file_output_test, a_derived_output_can_head_each_part) {
    {
        banner_file file(log_path(), {.max_size = 1});
        wxl::logging::logger log(file);

        log.info("one");
        log.info("two");
    }

    EXPECT_TRUE(contents_of(log_path()).starts_with("*** part ***\n"));
    EXPECT_TRUE(contents_of(log_path("app-part002.log")).starts_with("*** part ***\n"));
}

TEST_F(file_output_test, a_file_that_cannot_be_opened_says_so) {
    // A directory where the file should be: opening it as a file fails, and
    // failing to open the log is not something to find out about later.
    std::filesystem::create_directories(log_path());

    EXPECT_THROW(file_output file(log_path()), std::system_error);
}

}  // namespace
