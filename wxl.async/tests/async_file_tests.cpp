#include <gtest/gtest.h>

#include "platform.h"

#include <winioctl.h>

#include "loop_environment.h"
#include "test_directory.h"

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
        root_ = test_directory();

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
        wait_until([&] { return work.done(); });

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

namespace {

/// Two reads started one after the other, and awaited in the opposite order.
task reads_two_parts(path file_path, std::string& first, std::string& second) {
    async_file f = co_await async_file::open_read(file_path);

    char first_buffer[1000];
    char second_buffer[1000];

    auto a = f.read(first_buffer);
    auto b = f.read(second_buffer);

    second.assign(second_buffer, co_await b);
    first.assign(first_buffer, co_await a);
}

/// Into a buffer too large to be read on the thread that asks.
task reads_in_large_pieces(path file_path, std::string& out) {
    async_file f = co_await async_file::open_read(file_path);

    std::vector<char> buffer(async_file::direct_read_limit * 2);

    while (const std::size_t n = co_await f.read(
               std::as_writable_bytes(std::span(buffer.data(), buffer.size()))))
        out.append(buffer.data(), n);
}

/// Writes the whole of `content` in one write and reads it back in one read, each of
/// them more than one call to the system carries.
task writes_and_reads_in_one_go(path file_path, std::span<const std::byte> content,
                                std::vector<std::byte>& got, std::size_t& brought) {
    {
        async_file out = co_await async_file::create(file_path);

        EXPECT_EQ(co_await out.write(content), content.size());

        co_await out.close();
    }

    async_file in = co_await async_file::open_read(file_path);

    brought = co_await in.read(got);
}

/// Reads once into the whole buffer and says how much came.
task reads_once_into(path file_path, std::span<std::byte> into, std::size_t& brought) {
    async_file in = co_await async_file::open_read(file_path);

    brought = co_await in.read(into);
}

/// Writes nothing and reads into nothing.
task reads_and_writes_nothing(path file_path, std::size_t& written, std::size_t& read) {
    {
        async_file out = co_await async_file::create(file_path);

        written = co_await out.write(std::span<const std::byte>{});
        co_await out.close();
    }

    async_file in = co_await async_file::open_read(file_path);

    read = co_await in.read(std::span<std::byte>{});
}

/// Says, as it goes, that the operation carrying it has been deleted.
class deletion_mark
{
public:
    explicit deletion_mark(std::atomic<bool>& deleted) : deleted_(&deleted) {}

    deletion_mark(deletion_mark&& other) noexcept
        : deleted_(std::exchange(other.deleted_, nullptr)) {}

    deletion_mark& operator=(deletion_mark&&) = delete;

    ~deletion_mark() {
        if (deleted_) *deleted_ = true;
    }

private:
    std::atomic<bool>* deleted_;
};

/// Marks a file compressed, for a read that the system serves inside the call.
bool make_compressed(const path& file_path) {
    const HANDLE target = ::CreateFileW(file_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (target == INVALID_HANDLE_VALUE) return false;

    USHORT format = COMPRESSION_FORMAT_DEFAULT;
    DWORD returned = 0;

    const bool set = ::DeviceIoControl(target, FSCTL_SET_COMPRESSION, &format, sizeof(format),
                                       nullptr, 0, &returned, nullptr) != 0;

    ::CloseHandle(target);
    return set;
}

std::string large_content() {
    std::string content;

    for (int i = 0; content.size() < async_file::direct_read_limit * 5; ++i)
        content += "line " + std::to_string(i) + "\n";

    return content;
}

}  // namespace

TEST_F(AsyncFileTest, ReadsStartedTogetherAskForDifferentPlaces) {
    const std::string content = test_content();

    given_a_file(L"parts.bin", content);

    std::string first;
    std::string second;

    run(reads_two_parts(root_ / L"parts.bin", first, second));

    EXPECT_EQ(first, content.substr(0, 1000));
    EXPECT_EQ(second, content.substr(1000, 1000));
}

TEST_F(AsyncFileTest, ALargeReadGoesThroughTheWorkerAndBringsTheSame) {
    const std::string content = large_content();

    given_a_file(L"large.bin", content);

    std::string got;

    run(reads_in_large_pieces(root_ / L"large.bin", got));

    EXPECT_EQ(got, content);
}

TEST_F(AsyncFileTest, AReadOrAWriteLargerThanOneCallGoesInAChainOfThem) {
    // A megabyte over what one call takes: two calls each way.
    const std::size_t size = io_op::call_size + 1024 * 1024;

    std::vector<std::byte> content(size);

    // Word by word: a byte at a time is a second of a Debug build.
    std::uint64_t word = 0x9E37'79B97F4A7C15;

    for (std::size_t at = 0; at + 8 <= size; at += 8, word += (word << 5) | 1)
        std::memcpy(content.data() + at, &word, 8);

    std::vector<std::byte> got(size + 4096);
    std::size_t brought = 0;

#ifndef NDEBUG
    const std::size_t chained_before = io_op::debug.chained.load();
#endif

    run(writes_and_reads_in_one_go(root_ / L"chain.bin", content, got, brought));

    ASSERT_EQ(brought, size);
    EXPECT_EQ(std::memcmp(content.data(), got.data(), size), 0);

#ifndef NDEBUG
    // One call beyond the first for the write and one for the read.
    EXPECT_EQ(io_op::debug.chained.load() - chained_before, 2u);
#endif
}

TEST_F(AsyncFileTest, AnEmptyBufferReadsAndWritesNothing) {
    std::size_t written = 1;
    std::size_t read = 1;

    run(reads_and_writes_nothing(root_ / L"nothing.bin", written, read));

    EXPECT_EQ(written, 0u);
    EXPECT_EQ(read, 0u);
}

TEST_F(AsyncFileTest, AReadOfACompressedFileIsStartedOnTheWorker) {
    // The system serves such a read inside the call, so it must not be started where
    // the coroutine runs. Compression is NTFS's: a test directory on ReFS cannot hold
    // such a file, and the local application data folder, which is on the system
    // drive, is tried then; where neither can, there is nothing to check.
    const std::string content = test_content();
    path packed = root_ / L"packed.bin";
    path fallback;

    given_a_file(L"packed.bin", content);

    if (!make_compressed(packed)) {
        wchar_t local[MAX_PATH];

        if (!::GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH)) GTEST_SKIP();

        fallback = path(local) / L"Temp" /
                   (L"wxl.async.compressed-" + std::to_wstring(::GetCurrentProcessId()));

        std::filesystem::remove_all(fallback.native());
        ASSERT_TRUE(directory::create(fallback.c_str()));

        packed = fallback / L"packed.bin";

        file out = file::create(packed.c_str());

        ASSERT_TRUE(out.opened());
        ASSERT_EQ(out.write({reinterpret_cast<const std::byte*>(content.data()), content.size()}),
                  content.size());
        out.close();

        if (!make_compressed(packed)) {
            std::filesystem::remove_all(fallback.native());
            GTEST_SKIP() << "no compression here either";
        }
    }

#ifndef NDEBUG
    const std::size_t on_worker_before = io_op::debug.started_on_worker.load();
#endif

    std::vector<std::byte> got(content.size() + 16);
    std::size_t brought = 0;

    run(reads_once_into(packed, got, brought));

    if (!fallback.empty()) std::filesystem::remove_all(fallback.native());

    ASSERT_EQ(brought, content.size());
    EXPECT_EQ(std::memcmp(content.data(), got.data(), brought), 0);

#ifndef NDEBUG
    EXPECT_EQ(io_op::debug.started_on_worker.load() - on_worker_before, 1u);
#endif
}

TEST_F(AsyncFileTest, AFileMadeByAnOrphanGivenUpWhileItRunsIsLetGoOfOnTheSpot) {
    // The real thing behind the handle_like tests: a file made for writing, shared with
    // nobody, by an orphan given up while its body still runs. The body lets go of it
    // the moment the body ends, on its own thread, and the file can be made again.
    const path target = root_ / L"orphaned.bin";
    hevent made{true};
    hevent gate{true};
    std::atomic<bool> deleted{false};

    {
        auto making = sta_loop::async_call(
            orphanable, [&made, &gate, target, mark = deletion_mark(deleted)] {
                file out = file::create(target.c_str());

                made.set();
                gate.wait();

                return out;
            });

        made.wait();
    }

    gate.set();

    wait_until([&] { return deleted.load(); });

    file again = file::create(target.c_str());

    EXPECT_TRUE(again.opened()) << "the file given up was still held";
}

#ifndef WXL_ASYNC_TESTS_DISPATCHED

TEST_F(AsyncFileTest, AFileGivenUpOnceOpenCanBeAskedForAgainAtOnce) {
    // On the loop whose worker opens files, one at a time: by the time the worker is
    // inside the operation sent after the opening, the file is open, and nobody has
    // taken it. Made for writing, it is shared with nobody -- so that it can be made
    // again the moment the opening has been given up says the first one let go of it.
    const path target = root_ / L"again.bin";
    hevent passed{true};

    {
        auto opening = async_file::create(target);
        auto barrier = sta_loop::async_call([&passed] { passed.set(); });

        passed.wait();
    }

    file again = file::create(target.c_str());

    EXPECT_TRUE(again.opened()) << "the file given up was still held";

    EXPECT_EQ(sta_loop::run_pending(), 0u) << "a given-up operation resumed somebody";
}

#endif

// ---- Files whole, by name: read_all, write_all, exists ----------------------

namespace {

task reads_whole(path file_path, std::string& out) {
    out = co_await async_file::read_all(file_path);
}

task reads_whole_and_catches(path file_path, int& code) {
    try {
        co_await async_file::read_all(file_path);
        code = 0;
    } catch (const system_exception& failure) {
        code = failure.err_code();
    }
}

task writes_whole(path file_path, std::string bytes) {
    co_await async_file::write_all(file_path, std::move(bytes));
}

task asks_whether_exists(path file_path, bool& out) {
    out = co_await async_file::exists(file_path);
}

/// What is on the disk under this name, read the plain way: the check of a write.
std::string on_disk(const path& file_path) {
    file in = file::open_read(file_path.c_str());
    std::string bytes(static_cast<std::size_t>(in.size().value_or(0)), '\0');

    bytes.resize(in.read(std::as_writable_bytes(std::span(bytes))));

    return bytes;
}

}  // namespace

TEST_F(AsyncFileTest, ReadAllBringsTheWholeFileInOneOperation) {
    const std::string content = test_content();

    given_a_file(L"book.bin", content);

    std::string got;

    run(reads_whole(root_ / L"book.bin", got));

    EXPECT_EQ(got, content);
}

TEST_F(AsyncFileTest, ReadAllOfAnEmptyFileIsEmpty) {
    given_a_file(L"empty.bin", "");

    std::string got = "not empty";

    run(reads_whole(root_ / L"empty.bin", got));

    EXPECT_TRUE(got.empty());
}

TEST_F(AsyncFileTest, ReadAllOfAMissingFileNamesTheSystemsCode) {
    int code = -1;

    run(reads_whole_and_catches(root_ / L"missing.bin", code));

    EXPECT_EQ(code, ERROR_FILE_NOT_FOUND);
}

TEST_F(AsyncFileTest, WriteAllReplacesTheFileWholeAndLeavesNoTemporary) {
    given_a_file(L"state.txt", "the old state, longer than the new one");

    run(writes_whole(root_ / L"state.txt", "new"));

    EXPECT_EQ(on_disk(root_ / L"state.txt"), "new");
    EXPECT_FALSE(file::exists((root_ / L"state.txt.tmp").c_str()));
}

TEST_F(AsyncFileTest, WriteAllMakesTheDirectoriesOnTheWay) {
    const path target = root_ / L"deep" / L"er" / L"state.txt";

    run(writes_whole(target, "saved"));

    EXPECT_EQ(on_disk(target), "saved");
}

TEST_F(AsyncFileTest, WriteAllOfNothingLeavesAnEmptyFile) {
    given_a_file(L"state.txt", "something");

    run(writes_whole(root_ / L"state.txt", std::string()));

    EXPECT_TRUE(file::exists((root_ / L"state.txt").c_str()));
    EXPECT_TRUE(on_disk(root_ / L"state.txt").empty());
}

TEST_F(AsyncFileTest, ExistsTellsAFileFromADirectoryAndFromNothing) {
    given_a_file(L"here.bin", "x");

    bool is_file = false, is_directory = true, is_nothing = true;

    run(asks_whether_exists(root_ / L"here.bin", is_file));
    run(asks_whether_exists(root_, is_directory));
    run(asks_whether_exists(root_ / L"nowhere.bin", is_nothing));

    EXPECT_TRUE(is_file);
    EXPECT_FALSE(is_directory);
    EXPECT_FALSE(is_nothing);
}
