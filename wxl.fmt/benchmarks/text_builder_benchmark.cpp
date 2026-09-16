// Formatting into a buffer: the ways of doing it, measured against each other.
//
// This is the experiment wxl.fmt/doc/fmt.md asks for before any of it is
// believed, and its fifth point first: fmt::basic_memory_buffer with a wxl
// allocator is ready-made, so if text_buffer is no faster there is no reason
// for text_buffer to exist.
//
// Two axes are being measured, and they are independent: where the text goes
// (which buffer, on which allocator) and how the format string is read
// (walked at run time, or parsed by FMT_COMPILE at compile time). Every row
// varies one of them with the other held still, so the two effects can be
// read off separately and added.
//
// Two workloads, because they weigh those axes differently:
//
//  - a line at a time into a buffer that is reused. Nothing grows, so the
//    buffer axis is flat here and what is left is the formatting itself.
//  - one long document built in a single buffer. Growth is most of the cost,
//    which is where a buffer that can grow in place would show.

#include <chrono>
#include <cstdio>
#include <limits>
#include <format>
#include <string>
#include <vector>

#include <fmt/compile.h>
#include <fmt/format.h>

import wxl.core;
import wxl.fmt;

namespace {

using clock_t_ = std::chrono::steady_clock;
/// The best of several passes, not the average of them. What is being timed
/// takes the same work every pass, so anything above the fastest one is the
/// machine getting in the way -- a scheduler, another program, the clock
/// changing speed. Averaging keeps that noise in the answer; the minimum is
/// the closest to what the code actually costs.
template <typename Run>
double milliseconds(unsigned rounds, Run run) {
    constexpr unsigned passes = 5;

    double best = std::numeric_limits<double>::max();

    for (unsigned pass = 0; pass != passes; ++pass) {
        const auto started = clock_t_::now();

        for (unsigned round = 0; round != rounds; ++round) run();

        const auto took = clock_t_::now() - started;
        const double milli = std::chrono::duration<double, std::milli>(took).count();

        if (milli < best) best = milli;
    }

    return best;
}

void say(const char* what, double took, double against) {
    std::printf("    %-46s %8.2f ms   %5.2fx\n", what, took, against / took);
}

// ---- One line at a time, into a buffer that is kept ------------------------------------

void lines(unsigned rounds) {
    constexpr int page = 7;
    constexpr int count = 350;
    constexpr double share = 12.3456;
    const char* const title = "Мёртвые души";

    std::size_t sink = 0;

    std::string into_string;
    const double std_string = milliseconds(rounds, [&] {
        into_string.clear();
        std::format_to(std::back_inserter(into_string), "{}: page {} of {}, {:.1f}%", title, page,
                       count, share);
        sink += into_string.size();
    });

    const double std_fresh = milliseconds(rounds, [&] {
        const std::string line =
            std::format("{}: page {} of {}, {:.1f}%", title, page, count, share);
        sink += line.size();
    });

    fmt::memory_buffer fmt_buffer;
    const double fmt_memory = milliseconds(rounds, [&] {
        fmt_buffer.clear();
        fmt::format_to(fmt::appender(fmt_buffer), "{}: page {} of {}, {:.1f}%", title, page, count,
                       share);
        sink += fmt_buffer.size();
    });

    fmt::basic_memory_buffer<char, 256, wxl::core::sta_allocator<char>> pooled_buffer;
    const double fmt_pooled = milliseconds(rounds, [&] {
        pooled_buffer.clear();
        fmt::format_to(fmt::appender(pooled_buffer), "{}: page {} of {}, {:.1f}%", title, page,
                       count, share);
        sink += pooled_buffer.size();
    });

    wxl::core::text_builder<> ours;
    const double text_builder = milliseconds(rounds, [&] {
        ours.reset();
        ours.format("{}: page {} of {}, {:.1f}%", title, page, count, share);
        sink += ours.size();
    });

    wxl::core::text_builder<wxl::core::sta_allocator> ours_pooled;
    const double text_builder_pooled = milliseconds(rounds, [&] {
        ours_pooled.reset();
        ours_pooled.format("{}: page {} of {}, {:.1f}%", title, page, count, share);
        sink += ours_pooled.size();
    });

    fmt::basic_memory_buffer<char, 256, wxl::core::sta_allocator<char>> pooled_compiled;
    const double fmt_pooled_compiled = milliseconds(rounds, [&] {
        pooled_compiled.clear();
        fmt::format_to(fmt::appender(pooled_compiled),
                       FMT_COMPILE("{}: page {} of {}, {:.1f}%"), title, page, count, share);
        sink += pooled_compiled.size();
    });

    wxl::core::text_builder<wxl::core::sta_allocator> compiled;
    const double text_builder_compiled = milliseconds(rounds, [&] {
        compiled.reset();
        compiled.format(FMT_COMPILE("{}: page {} of {}, {:.1f}%"), title, page, count, share);
        sink += compiled.size();
    });

    std::printf("one line, buffer reused  x%u\n", rounds);
    say("std::format into a fresh std::string", std_fresh, std_string);
    say("std::format_to back_inserter(std::string)", std_string, std_string);
    say("fmt::memory_buffer", fmt_memory, std_string);
    say("fmt::basic_memory_buffer<sta_allocator>", fmt_pooled, std_string);
    say("wxl::core::text_builder<std::allocator>", text_builder, std_string);
    say("wxl::core::text_builder<sta_allocator>", text_builder_pooled, std_string);
    say("fmt::basic_memory_buffer<sta_allocator> + COMPILE", fmt_pooled_compiled, std_string);
    say("wxl::core::text_builder<sta_allocator> + COMPILE", text_builder_compiled, std_string);

    if (sink == 0) std::printf("");
}

// ---- One long document, built in a single buffer -------------------------------------

void document(unsigned rounds, unsigned records) {
    std::size_t sink = 0;

    const double std_string = milliseconds(rounds, [&] {
        std::string out;
        for (unsigned at = 0; at != records; ++at)
            std::format_to(std::back_inserter(out), "<r i=\"{}\" v=\"{:.3f}\"/>", at, at * 1.5);
        sink += out.size();
    });

    const double fmt_memory = milliseconds(rounds, [&] {
        fmt::memory_buffer out;
        for (unsigned at = 0; at != records; ++at)
            fmt::format_to(fmt::appender(out), "<r i=\"{}\" v=\"{:.3f}\"/>", at, at * 1.5);
        sink += out.size();
    });

    const double fmt_pooled = milliseconds(rounds, [&] {
        fmt::basic_memory_buffer<char, 256, wxl::core::sta_allocator<char>> out;
        for (unsigned at = 0; at != records; ++at)
            fmt::format_to(fmt::appender(out), "<r i=\"{}\" v=\"{:.3f}\"/>", at, at * 1.5);
        sink += out.size();
    });

    const double ours_heap = milliseconds(rounds, [&] {
        wxl::core::text_builder<> out;
        for (unsigned at = 0; at != records; ++at)
            out.format("<r i=\"{}\" v=\"{:.3f}\"/>", at, at * 1.5);
        sink += out.size();
    });

    const double ours_pooled = milliseconds(rounds, [&] {
        wxl::core::text_builder<wxl::core::sta_allocator> out;
        for (unsigned at = 0; at != records; ++at)
            out.format("<r i=\"{}\" v=\"{:.3f}\"/>", at, at * 1.5);
        sink += out.size();
    });

    const double fmt_pooled_compiled = milliseconds(rounds, [&] {
        fmt::basic_memory_buffer<char, 256, wxl::core::sta_allocator<char>> out;
        for (unsigned at = 0; at != records; ++at)
            fmt::format_to(fmt::appender(out), FMT_COMPILE("<r i=\"{}\" v=\"{:.3f}\"/>"), at,
                           at * 1.5);
        sink += out.size();
    });

    const double ours_compiled = milliseconds(rounds, [&] {
        wxl::core::text_builder<wxl::core::sta_allocator> out;
        for (unsigned at = 0; at != records; ++at)
            out.format(FMT_COMPILE("<r i=\"{}\" v=\"{:.3f}\"/>"), at, at * 1.5);
        sink += out.size();
    });

    std::printf("\n%u records, one buffer, built from empty  x%u\n", records, rounds);
    say("std::format_to back_inserter(std::string)", std_string, std_string);
    say("fmt::memory_buffer", fmt_memory, std_string);
    say("fmt::basic_memory_buffer<sta_allocator>", fmt_pooled, std_string);
    say("wxl::core::text_builder<std::allocator>", ours_heap, std_string);
    say("wxl::core::text_builder<sta_allocator>", ours_pooled, std_string);
    say("fmt::basic_memory_buffer<sta_allocator> + COMPILE", fmt_pooled_compiled, std_string);
    say("wxl::core::text_builder<sta_allocator> + COMPILE", ours_compiled, std_string);

    if (sink == 0) std::printf("");
}

}  // namespace

int main() {
    lines(100000);
    document(60, 20000);

    return 0;
}
