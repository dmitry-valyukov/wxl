#include <gtest/gtest.h>

#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

import wxl.core;
import wxl.logging;

#include "recording_output.h"

using wxl::logging::async_logger;
using wxl::logging::async_output;
using wxl::logging::entry_pool;
using wxl::logging::entry_ptr;
using wxl::logging::severity;

namespace {

/// A sink that keeps the lines instead of writing them, for the half of the
/// logger that has nothing to do with threads.
class collecting_sink final : public wxl::logging::async_sink
{
public:
    void enqueue(entry_ptr& entry) override {
        lines.emplace_back(entry->text());
        levels.push_back(entry->level);

        entry.reset();  // back to the pool it came from, as a real sink does
    }

    std::vector<std::string> lines;
    std::vector<severity> levels;
};

/// Puts one ready-made entry into the queue, stamped as the caller says.
/// Timestamps in the future are never due, so a test can decide for itself when
/// the lines come out.
void enqueue_line(async_output& writer, entry_pool& pool, const wxl::logging::log_time time,
                  const std::string_view text) {
    entry_ptr entry(pool.get());

    entry->reset();
    entry->time = time;
    entry->level = severity::info;
    entry->buffer.append(text);

    writer.enqueue(entry);
}

TEST(async_logger, the_lines_go_to_the_sink) {
    collecting_sink sink;
    async_logger log(sink, severity::debug);

    log.info("one {}", 1);
    log.error("two");

    ASSERT_EQ(sink.lines.size(), 2u);
    EXPECT_TRUE(sink.lines[0].ends_with("one 1\n"));
    EXPECT_TRUE(sink.lines[1].ends_with("two\n"));
    EXPECT_EQ(sink.levels[1], severity::error);
}

TEST(async_logger, a_message_below_the_level_never_reaches_the_sink) {
    collecting_sink sink;
    async_logger log(sink, severity::info);

    log.debug("dropped");
    log.info("kept");

    EXPECT_EQ(sink.lines.size(), 1u);
}

TEST(async_logger, a_warmed_pool_hands_the_same_entries_back) {
    collecting_sink sink;
    async_logger log(sink, severity::info, 8);

    for (int line = 0; line < 100; ++line) log.info("line {}", line);

    EXPECT_EQ(sink.lines.size(), 100u);
    EXPECT_TRUE(sink.lines.back().ends_with("line 99\n"));
}

TEST(async_output, everything_enqueued_is_written_by_the_time_a_flush_returns) {
    recording_output kept;
    async_output writer;

    writer.subscribe(kept);
    writer.start_async().get();

    {
        async_logger log(writer, severity::debug);

        for (int line = 0; line < 50; ++line) log.info("line {}", line);
    }

    writer.flush_all();

    EXPECT_EQ(kept.size(), 50u);
}

TEST(async_output, the_window_puts_the_lines_back_in_the_order_they_were_made) {
    recording_output kept;
    entry_pool pool;
    async_output writer;

    writer.subscribe(kept);
    writer.start_async().get();

    // Stamped ahead of the clock, so nothing comes due on its own and the only
    // thing deciding the order is the sort.
    const auto later = wxl::logging::log_clock::now() + std::chrono::hours(1);

    enqueue_line(writer, pool, later + std::chrono::milliseconds(30), "third\n");
    enqueue_line(writer, pool, later + std::chrono::milliseconds(10), "first\n");
    enqueue_line(writer, pool, later + std::chrono::milliseconds(20), "second\n");

    writer.flush_all();

    const auto lines = kept.lines();

    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "first\n");
    EXPECT_EQ(lines[1], "second\n");
    EXPECT_EQ(lines[2], "third\n");
}

TEST(async_output, a_line_still_inside_its_window_waits_for_a_flush_of_everything) {
    recording_output kept;
    entry_pool pool;
    async_output writer;

    writer.subscribe(kept);
    writer.start_async().get();

    enqueue_line(writer, pool, wxl::logging::log_clock::now() + std::chrono::hours(1), "later\n");

    writer.flush_window();

    EXPECT_EQ(kept.size(), 0u);

    writer.flush_all();

    EXPECT_EQ(kept.size(), 1u);
}

TEST(async_output, stopping_writes_out_what_is_still_held) {
    recording_output kept;
    entry_pool pool;

    {
        async_output writer;

        writer.subscribe(kept);
        writer.start_async().get();

        enqueue_line(writer, pool, wxl::logging::log_clock::now() + std::chrono::hours(1),
                     "the last thing it said\n");

        // No flush: the destructor stops the thread, and a log that dropped its
        // last line would do it exactly when it mattered.
    }

    ASSERT_EQ(kept.size(), 1u);
    EXPECT_EQ(kept.lines().front(), "the last thing it said\n");
}

TEST(async_output, several_threads_write_into_one_log) {
    constexpr int threads = 4;
    constexpr int lines_each = 500;

    recording_output kept;
    async_output writer(wxl::core::duration::from_ms(5));

    writer.subscribe(kept);
    writer.start_async().get();

    std::vector<std::thread> writers;

    for (int number = 0; number < threads; ++number)
        writers.emplace_back([&writer, number] {
            // A logger per thread, which is the arrangement the library asks
            // for: its own buffer, its own pool, nothing shared but the queue.
            async_logger log(writer, severity::debug, 16);

            for (int line = 0; line < lines_each; ++line) log.info("{}:{}", number, line);
        });

    for (std::thread& one : writers) one.join();

    writer.flush_all();

    const auto lines = kept.lines();

    ASSERT_EQ(lines.size(), std::size_t(threads * lines_each));

    // Nothing torn and nothing repeated: every line is whole and its own.
    std::set<std::string> messages;

    for (std::string_view line : lines) {
        EXPECT_TRUE(line.ends_with("\n"));
        messages.insert(std::string(line));
    }

    EXPECT_EQ(messages.size(), lines.size());
}

TEST(async_output, the_lines_come_out_in_the_order_they_were_made) {
    recording_output kept;
    // A window long enough that nothing comes due while the threads are running:
    // every line is then sorted by the flush at the end, and the check is about
    // the sort rather than about how the machine happened to schedule.
    async_output writer(wxl::core::duration::from_sec(1));

    writer.subscribe(kept);
    writer.start_async().get();

    std::vector<std::thread> writers;

    for (int number = 0; number < 4; ++number)
        writers.emplace_back([&writer, number] {
            async_logger log(writer, severity::debug, 16);

            for (int line = 0; line < 200; ++line) {
                log.info("{}:{}", number, line);
                std::this_thread::yield();
            }
        });

    for (std::thread& one : writers) one.join();

    writer.flush_all();

    const auto times = kept.times();

    ASSERT_FALSE(times.empty());

    for (std::size_t at = 1; at < times.size(); ++at)
        EXPECT_LE(times[at - 1], times[at]) << "line " << at << " was made before the one before it";
}

}  // namespace
