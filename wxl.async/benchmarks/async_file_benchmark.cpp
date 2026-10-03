// What one read of a file costs, by how it is carried out: on the calling thread, on the
// worker through the queue, and as an overlapped operation started on either thread and
// finished through the port. One cached file, read through in chunks of three sizes.
//
// Usage: wxl.async.async-file-benchmark [rounds [file]]
//        `file` is where the 4 MB file is made; it is removed afterwards.

#include "platform.h"

import std;
import wxl.core;
import wxl.async;

namespace {

using namespace wxl::async;
using wxl::core::file;

using bench_clock = std::chrono::steady_clock;

constexpr std::size_t file_size = 4 * 1024 * 1024;

struct outcome {
    double microseconds_per_chunk = 0;
    std::size_t chunks = 0;
    std::size_t over_at_once = 0;
};

using reader = task (*)(const wchar_t*, std::span<std::byte>, outcome&);

/// The read with no loop under it: what the rest is compared with.
task on_the_calling_thread(const wchar_t* path, std::span<std::byte> buffer, outcome& out) {
    file f = file::open_read(path);

    while (f.read(buffer)) ++out.chunks;

    co_return;
}

/// A blocking read carried to the worker and back.
task on_the_worker(const wchar_t* path, std::span<std::byte> buffer, outcome& out) {
    file f = file::open_read(path);

    while (co_await sta_loop::async_call([&] { return f.read(buffer); })) ++out.chunks;
}

template <bool started_here>
task overlapped(const wchar_t* path, std::span<std::byte> buffer, outcome& out) {
    file f = file::open_read_overlapped(path);
    const bool skips_port = sta_loop::port().attach(f.native_handle());

    for (std::uint64_t at = 0;; at += buffer.size()) {
        std::unique_ptr<io_op> op(new io_op(io_op::kind::read, f.native_handle(), buffer.data(),
                                            buffer.size(), at, skips_port));

        auto reading = started_here
                           ? sta_loop::async_start(std::move(op))
                           : sta_loop::async_run(
                                 std::unique_ptr<async_op_t<std::size_t>>(std::move(op)));

        out.over_at_once += reading.await_ready();

        if (co_await reading == 0) break;

        ++out.chunks;
    }
}

outcome measure(reader read, const wchar_t* path, std::size_t chunk, int rounds) {
    std::vector<std::byte> buffer(chunk);
    outcome best{std::numeric_limits<double>::max()};

    for (int round = 0; round < rounds; ++round) {
        outcome out;
        const auto started = bench_clock::now();

        task work = read(path, buffer, out);

        sta_loop::run_until([&] { return work.done(); });
        work.result();

        const std::chrono::duration<double, std::micro> took = bench_clock::now() - started;

        out.microseconds_per_chunk = took.count() / static_cast<double>(out.chunks);

        if (out.microseconds_per_chunk < best.microseconds_per_chunk) best = out;
    }

    return best;
}

void row(const char* name, reader read, const wchar_t* path, int rounds) {
    std::printf("  %-34s", name);

    for (const std::size_t chunk : {std::size_t{4} * 1024, std::size_t{64} * 1024,
                                    std::size_t{1024} * 1024}) {
        const outcome out = measure(read, path, chunk, rounds);

        std::printf(" %9.1f %5.0f%%", out.microseconds_per_chunk,
                    100.0 * static_cast<double>(out.over_at_once) /
                        static_cast<double>(out.chunks + 1));
    }

    std::putchar('\n');
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
    const int rounds = argc > 1 ? static_cast<int>(std::wcstol(argv[1], nullptr, 10)) : 5;
    const std::wstring path = argc > 2 ? argv[2] : L"async-file-benchmark.bin";

    {
        file made = file::create(path.c_str());
        const std::vector<std::byte> content(file_size, std::byte{42});

        if (made.write(content) != content.size()) {
            std::printf("the file could not be made\n");
            return 1;
        }
    }

    sta_loop::start("benchmark: I/O");

    std::printf("a cached 4 MB file read in chunks -- microseconds per chunk, best of %d rounds,\n"
                "and how many of the reads were over before they were awaited\n\n",
                rounds);
    std::printf("  %-34s %9s %6s %9s %6s %9s %6s\n", "", "4 KB", "", "64 KB", "", "1 MB", "");

    row("on the calling thread", on_the_calling_thread, path.c_str(), rounds);
    row("blocking, on the worker", on_the_worker, path.c_str(), rounds);
    row("overlapped, started here", overlapped<true>, path.c_str(), rounds);
    row("overlapped, started on the worker", overlapped<false>, path.c_str(), rounds);

    sta_loop::stop();

    ::DeleteFileW(path.c_str());
}
