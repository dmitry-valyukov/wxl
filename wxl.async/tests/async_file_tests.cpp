#include <gtest/gtest.h>

#include "sta_pool.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

/// The coroutine the whole scheme exists for: opening and reading a file
/// without a single blocking call on the thread it is written on.
task read_all(path file_path, std::string& out, std::thread::id& worker_thread) {
    // Proof, from inside the coroutine, that the other side of the loop really
    // is another thread -- and the shortest possible use of async_call.
    worker_thread =
        co_await sta_loop::async_call([] { return std::this_thread::get_id(); });

    async_file f = co_await async_file::open_read(file_path);

    std::byte buffer[1024];

    while (const std::size_t n = co_await f.read(buffer))
        out.append(reinterpret_cast<const char*>(buffer), n);
}

/// The same through a character buffer: the array overload of read() spells
/// the byte view itself, and what comes out appends without a cast.
task read_all_as_text(path file_path, std::string& out) {
    async_file f = co_await async_file::open_read(file_path);

    char buffer[1024];

    while (const std::size_t n = co_await f.read(buffer)) out.append(buffer, n);
}

/// The same, for a file that is not there: the failure happens on the worker
/// thread and is caught here, at the co_await, as an ordinary exception.
task open_and_catch(path file_path, std::string& message) {
    try {
        co_await async_file::open_read(file_path);
        message = "no exception";
    } catch (const system_exception& e) {
        message = e.what();
    }
}

/// The writing half, through the same machinery: create, write, flush, close,
/// and then read the whole thing back and compare.
task write_then_read_back(path file_path, std::string_view content,
                          std::string& got, std::uint64_t& reported_size) {
    {
        async_file out = co_await async_file::create(file_path);

        const std::size_t written = co_await out.write(
            std::span(reinterpret_cast<const std::byte*>(content.data()), content.size()));

        EXPECT_EQ(written, content.size());

        co_await out.flush();
        co_await out.close();
    }

    async_file in = co_await async_file::open_read(file_path);

    reported_size = co_await in.size();

    got.resize(static_cast<std::size_t>(reported_size));

    const std::size_t read =
        co_await in.read(std::span(reinterpret_cast<std::byte*>(got.data()), got.size()));

    got.resize(read);
}

/// Longer than the buffer the reading coroutine uses, so the loop goes round
/// more than once and the whole send-execute-return trip happens several times.
std::string test_content() {
    std::string content;

    for (int i = 0; content.size() < 5000; ++i) content += "line " + std::to_string(i) + "\n";

    return content;
}

class AsyncFileTest : public ::testing::Test
{
protected:
    void SetUp() override {
        root_ = path(std::filesystem::temp_directory_path().wstring()) / L"wxl_async_file_tests";

        std::filesystem::remove_all(root_.native());

        ASSERT_TRUE(directory::create(root_.c_str()));
    }

    void TearDown() override {
        std::filesystem::remove_all(root_.native());
    }

    void given_a_file(const wchar_t* name, std::string_view content) {
        file out = file::create((root_ / name).c_str());

        ASSERT_TRUE(out.opened());
        ASSERT_EQ(out.write({reinterpret_cast<const std::byte*>(content.data()), content.size()}),
                  content.size());
    }

    void run(task work) {
        sta_loop::run_until([&] { return work.done(); });

        work.result();
    }

    path root_;
};

}  // namespace

TEST_F(AsyncFileTest, ReadsAFileInACoroutine) {
    const std::string content = test_content();

    given_a_file(L"book.bin", content);

    std::string got;
    std::thread::id worker_thread;

    run(read_all(root_ / L"book.bin", got, worker_thread));

    EXPECT_EQ(got, content);
    EXPECT_NE(worker_thread, std::thread::id{});
    EXPECT_NE(worker_thread, std::this_thread::get_id());
}

TEST_F(AsyncFileTest, ReadsIntoACharacterBuffer) {
    const std::string content = test_content();

    given_a_file(L"text.bin", content);

    std::string got;

    run(read_all_as_text(root_ / L"text.bin", got));

    EXPECT_EQ(got, content);
}

TEST_F(AsyncFileTest, ReadsAnEmptyFileAsNothingAtAll) {
    given_a_file(L"empty.bin", "");

    std::string got;
    std::thread::id worker_thread;

    run(read_all(root_ / L"empty.bin", got, worker_thread));

    EXPECT_TRUE(got.empty());
}

TEST_F(AsyncFileTest, ExceptionFromTheWorkerArrivesAtTheCoAwait) {
    std::string message;

    run(open_and_catch(root_ / L"no_such_file", message));

    EXPECT_TRUE(message.starts_with("CreateFileW")) << message;
}

TEST_F(AsyncFileTest, WritesAFileAndReadsItBack) {
    const std::string content = test_content();

    std::string got;
    std::uint64_t reported_size = 0;

    run(write_then_read_back(root_ / L"written.bin", content, got, reported_size));

    EXPECT_EQ(reported_size, content.size());
    EXPECT_EQ(got, content);
}

TEST_F(AsyncFileTest, APathGivenToAnOperationNeedNotOutliveTheStatement) {
    // The operation keeps its own copy of the path, so one that dies with the
    // statement that started the coroutine is still safe. Borrowing it, which is
    // what this code did first, left the worker reading freed characters.
    const std::string content = "brief";

    given_a_file(L"brief.bin", content);

    std::string got;
    std::thread::id worker_thread;

    task work = read_all(path(root_.native()) / L"brief.bin", got, worker_thread);

    run(std::move(work));

    EXPECT_EQ(got, content);
}
