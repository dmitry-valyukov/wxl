// Reads the kernel is holding: what arrives through the port, and what giving such a
// read up does. A pipe nobody writes to stands for the slow device -- a read from it
// stays with the kernel until something is written or the read is cancelled.
#include <gtest/gtest.h>

#include "platform.h"

#include "loop_environment.h"
#include "test_directory.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

/// The writing end of a pipe, which writes only when told to.
class quiet_pipe
{
public:
    quiet_pipe() {
        static int made = 0;

        name_ = L"\\\\.\\pipe\\wxl-async-file-tests-" + std::to_wstring(::GetCurrentProcessId()) +
                L"-" + std::to_wstring(++made);

        server_ = ::CreateNamedPipeW(name_.c_str(), PIPE_ACCESS_OUTBOUND,
                                     PIPE_TYPE_BYTE | PIPE_WAIT, 1, 4096, 4096, 0, nullptr);
    }

    ~quiet_pipe() {
        if (server_ != INVALID_HANDLE_VALUE) ::CloseHandle(server_);
    }

    bool made() const noexcept { return server_ != INVALID_HANDLE_VALUE; }

    path name() const { return path(name_); }

    bool write(std::string_view text) const noexcept {
        return write(std::as_bytes(std::span(text)));
    }

    bool write(std::span<const std::byte> bytes) const noexcept {
        DWORD written = 0;

        return ::WriteFile(server_, bytes.data(), static_cast<DWORD>(bytes.size()), &written,
                           nullptr) &&
               written == bytes.size();
    }

private:
    std::wstring name_;
    HANDLE server_ = INVALID_HANDLE_VALUE;
};

/// Writes to the pipe after a while unless told not to, so that a read which was to be
/// cancelled and was not fails its test instead of hanging the binary.
class rescue
{
public:
    explicit rescue(const quiet_pipe& pipe)
        : thread_([this, &pipe] {
              if (done_.try_acquire_for(std::chrono::seconds(10))) return;

              needed_ = true;
              pipe.write("rescue");
          }) {}

    /// \return whether the read had to be let go by writing to the pipe.
    bool was_needed() {
        done_.release();
        thread_.join();

        return needed_;
    }

private:
    std::binary_semaphore done_{0};
    std::atomic<bool> needed_{false};
    std::jthread thread_;
};

task<> reads_once(path pipe_name, std::string& got, bool& reading) {
    async_file in = co_await async_file::open_read(pipe_name);

    char buffer[64];

    auto read = in.read(buffer);

    reading = true;
    got.assign(buffer, co_await read);
}

/// The code from the discussion, on handles of its own: the first read fails, and the
/// frame unwinds with the second one in the kernel's hands, waiting to write into
/// `second_buffer`.
task<> first_read_fails_second_is_with_the_kernel(path refusing, path pipe_name) {
    async_file out = co_await async_file::create(refusing);
    async_file in = co_await async_file::open_read(pipe_name);

    std::byte first_buffer[64];
    std::byte second_buffer[64];

    // A file made for writing refuses to be read.
    auto a = out.read(first_buffer);
    auto b = in.read(second_buffer);

    co_await a;
    co_await b;
}

/// Opens, lets the test write, and only then reads: the data is in the pipe before the
/// read starts, so the system can finish the read inside the call.
task<> reads_what_is_already_there(path pipe_name, bool& opened, hevent& written, bool& inline_done,
                                 std::string& got) {
    async_file in = co_await async_file::open_read(pipe_name);

    opened = true;

    // The worker stands here until the test has written.
    co_await sta_loop::async_call([&written] { written.wait(); });

    char buffer[64];

    auto read = in.read(buffer);

    inline_done = read.ready();
    got.assign(buffer, co_await read);
}

/// A read of more than one call carries, dropped once the first call has brought all it
/// asked for and the second is in the kernel's hands.
task<> reads_a_chain_and_is_dropped(path pipe_name, std::vector<std::byte>& buffer, bool& reading) {
    async_file in = co_await async_file::open_read(pipe_name);

    auto read = in.read(buffer);

    reading = true;
    co_await read;

    ADD_FAILURE() << "resumed after its task was dropped";
}

task<> reads_and_is_dropped(path pipe_name, bool& reading) {
    async_file in = co_await async_file::open_read(pipe_name);

    std::byte buffer[64];

    auto read = in.read(buffer);

    reading = true;
    co_await read;

    ADD_FAILURE() << "resumed after its task was dropped";
}

/// A pipe whose far end takes a little and then no more: a write of more than it holds
/// stays inside the call until somebody reads, or until the call is cut short.
class narrow_pipe
{
public:
    static constexpr DWORD capacity = 4096;

    narrow_pipe() {
        static int made = 0;

        const std::wstring name = L"\\\\.\\pipe\\wxl-async-narrow-" +
                                  std::to_wstring(::GetCurrentProcessId()) + L"-" +
                                  std::to_wstring(++made);

        reader_ = ::CreateNamedPipeW(name.c_str(), PIPE_ACCESS_INBOUND, PIPE_TYPE_BYTE | PIPE_WAIT, 1,
                                     0, capacity, 0, nullptr);
        writer_ = ::CreateFileW(name.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    }

    ~narrow_pipe() {
        if (writer_ != INVALID_HANDLE_VALUE) ::CloseHandle(writer_);
        if (reader_ != INVALID_HANDLE_VALUE) ::CloseHandle(reader_);
    }

    bool made() const noexcept {
        return reader_ != INVALID_HANDLE_VALUE && writer_ != INVALID_HANDLE_VALUE;
    }

    /// For a blocking write, on whatever thread makes it.
    HANDLE writer() const noexcept { return writer_; }

    /// Takes one byte, waiting for it: once it is here, the writer is inside its call.
    bool take_one() const noexcept {
        char byte = 0;
        DWORD got = 0;

        return ::ReadFile(reader_, &byte, 1, &got, nullptr) && got == 1;
    }

    /// Takes whatever is written until the writer is done, letting it through.
    void drain() const noexcept {
        char buffer[capacity];
        DWORD got = 0;

        while (::ReadFile(reader_, buffer, sizeof(buffer), &got, nullptr) && got != 0) {}
    }

private:
    HANDLE reader_ = INVALID_HANDLE_VALUE;
    HANDLE writer_ = INVALID_HANDLE_VALUE;
};

/// Says, as it goes, that the operation carrying it has been deleted: it rides in the
/// capture of the body.
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

/// Gives an orphan up the moment it is started, over and over, and between the orphans
/// awaits a blocking read carried out where they are -- which has to come through every
/// time: cutting short is for the call of the operation given up, and no other.
task<> reads_among_orphans_given_up(path file_path, int rounds, int& cut_short, int& read) {
    for (int round = 0; round < rounds; ++round) {
        { auto given_up = async_directory::exists(file_path); }

        const DWORD outcome = co_await sta_loop::async_call(orphanable, [file_path] {
            file f = file::open_read(file_path.c_str());

            if (!f.opened()) return ::GetLastError();

            std::byte buffer[4096];

            ::SetLastError(ERROR_SUCCESS);
            f.read(buffer);

            return ::GetLastError();
        });

        if (outcome == ERROR_OPERATION_ABORTED)
            ++cut_short;
        else if (outcome == ERROR_SUCCESS)
            ++read;
    }
}

class AsyncFilePipeTest : public ::testing::Test
{
protected:
    void SetUp() override {
        ASSERT_TRUE(pipe_.made());

        root_ = test_directory();

        std::filesystem::remove_all(root_.native());

        ASSERT_TRUE(directory::create(root_.c_str()));
    }

    void TearDown() override { std::filesystem::remove_all(root_.native()); }

    quiet_pipe pipe_;
    path root_;
};

}  // namespace

TEST_F(AsyncFilePipeTest, AReadTheKernelHeldArrivesThroughThePort) {
    std::string got;
    bool reading = false;

    task<> work = reads_once(pipe_.name(), got, reading);

    wait_until([&] { return reading; });

    EXPECT_FALSE(work.done());
    ASSERT_TRUE(pipe_.write("hello"));

    wait_until([&] { return work.done(); });
    work.result();

    EXPECT_EQ(got, "hello");
}

TEST_F(AsyncFilePipeTest, AFrameUnwindingPastAReadInTheKernelCancelsIt) {
    rescue watchdog(pipe_);

    task<> work = first_read_fails_second_is_with_the_kernel(root_ / L"refusing.bin", pipe_.name());

    wait_until([&] { return work.done(); });

    EXPECT_FALSE(watchdog.was_needed()) << "the read was waited for instead of cancelled";

    try {
        work.result();
        ADD_FAILURE() << "reading a file made for writing did not fail";
    } catch (const system_exception& e) {
        EXPECT_TRUE(std::string_view(e.what()).starts_with("ReadFile")) << e.what();
    }

    EXPECT_EQ(sta_loop::run_pending(), 0u) << "a given-up operation resumed somebody";
}

TEST_F(AsyncFilePipeTest, DroppingATaskSuspendedOnAReadInTheKernelCancelsIt) {
    rescue watchdog(pipe_);
    bool reading = false;

    {
        task<> work = reads_and_is_dropped(pipe_.name(), reading);

        wait_until([&] { return reading; });

        EXPECT_FALSE(work.done());
    }

    EXPECT_FALSE(watchdog.was_needed()) << "the read was waited for instead of cancelled";
    EXPECT_EQ(sta_loop::run_pending(), 0u) << "a given-up operation resumed somebody";
}

TEST(OrphanTest, GivingUpAnOrphanCutsShortTheCallItStandsIn) {
    narrow_pipe pipe;

    ASSERT_TRUE(pipe.made());

    static constexpr DWORD not_yet = ~DWORD{0};

    std::atomic<DWORD> outcome{not_yet};
    std::atomic<bool> deleted{false};
    hevent left{true};

    // Lets the write through after a while unless told not to, so that a write which
    // was to be cut short and was not fails the test instead of hanging the binary.
    std::binary_semaphore done{0};
    std::jthread rescue([&] {
        if (!done.try_acquire_for(std::chrono::seconds(10))) pipe.drain();
    });

    {
        auto writing = sta_loop::async_call(
            orphanable, [writer = pipe.writer(), &outcome, &left, mark = deletion_mark(deleted)] {
                static const std::vector<char> much(1024 * 1024);

                DWORD written = 0;

                outcome = ::WriteFile(writer, much.data(), static_cast<DWORD>(much.size()),
                                      &written, nullptr)
                              ? ERROR_SUCCESS
                              : ::GetLastError();

                left.set();
            });

        // A byte of the megabyte has come through, so the body is inside its write,
        // and the pipe holds too little for it to leave.
        ASSERT_TRUE(pipe.take_one());

        EXPECT_EQ(outcome.load(), not_yet);
    }

    left.wait();
    done.release();

    EXPECT_EQ(outcome.load(), static_cast<DWORD>(ERROR_OPERATION_ABORTED));

    wait_until([&] { return deleted.load(); });
}

TEST_F(AsyncFilePipeTest, GivingAnOrphanUpNeverCutsShortACallOfAnotherOperation) {
    constexpr int rounds = 2000;

    const path target = root_ / L"among.bin";

    {
        file made = file::create(target.c_str());
        const std::vector<std::byte> content(4096, std::byte{7});

        ASSERT_TRUE(made.opened());
        ASSERT_EQ(made.write(content), content.size());
    }

    int cut_short = 0;
    int read = 0;

    task<> work = reads_among_orphans_given_up(target, rounds, cut_short, read);

    wait_until([&] { return work.done(); });
    work.result();

    EXPECT_EQ(cut_short, 0);
    EXPECT_EQ(read, rounds);
}

TEST(OrphanTest, ABodyOfSeveralCallsStopsAtTheOneCutShort) {
    narrow_pipe pipe;

    ASSERT_TRUE(pipe.made());

    std::atomic<int> calls{0};
    hevent left{true};

    std::binary_semaphore done{0};
    std::jthread rescue([&] {
        if (!done.try_acquire_for(std::chrono::seconds(10))) pipe.drain();
    });

    {
        auto writing = sta_loop::async_call(
            orphanable, [writer = pipe.writer(), &calls, &left](const orphan_stage& stage) {
                static const std::vector<char> much(1024 * 1024);

                // Three writes, each of more than the pipe holds; the body asks between
                // them whether it is still wanted.
                for (int call = 0; call < 3 && !stage.given_up(); ++call) {
                    DWORD written = 0;

                    ++calls;
                    ::WriteFile(writer, much.data(), static_cast<DWORD>(much.size()), &written,
                                nullptr);
                }

                left.set();
            });

        ASSERT_TRUE(pipe.take_one());
    }

    left.wait();
    done.release();

    EXPECT_EQ(calls.load(), 1);
}

TEST_F(AsyncFilePipeTest, AReadOfDataAlreadyInThePipeNeedNotSuspend) {
    bool opened = false;
    hevent written{true};
    bool inline_done = false;
    std::string got;

    task<> work = reads_what_is_already_there(pipe_.name(), opened, written, inline_done, got);

    wait_until([&] { return opened; });

    ASSERT_TRUE(pipe_.write("there"));
    written.set();

    wait_until([&] { return work.done(); });
    work.result();

    EXPECT_EQ(got, "there");

    // Whether the system finished the read inside the call is its choice; the test only
    // reports it, since that is the one path a file on this machine never takes.
    std::cout << "[          ] the read of data already in the pipe "
              << (inline_done ? "finished inside the call" : "went through the port") << std::endl;
}

TEST_F(AsyncFilePipeTest, AFrameUnwindingBetweenTwoCallsOfAChainCancelsTheSecond) {
    // The read asks for more than one call carries. The first call is answered in full
    // by one write of exactly that much, the chain goes on to a second call, and that one
    // waits in the kernel for data that never comes -- until the task is dropped.
    std::vector<std::byte> buffer(io_op::call_size + 1024 * 1024);
    const std::vector<std::byte> first_call(io_op::call_size, std::byte{5});
    bool reading = false;

#ifndef NDEBUG
    const std::size_t chained_before = io_op::debug.chained.load();
#endif

    std::binary_semaphore done{0};
    std::jthread watchdog([&] {
        if (!done.try_acquire_for(std::chrono::seconds(20))) pipe_.write("rescue");
    });

    {
        task<> work = reads_a_chain_and_is_dropped(pipe_.name(), buffer, reading);

        wait_until([&] { return reading; });

        // On a thread of its own: the write returns once the reader has taken it all,
        // and a reader that took less would leave it standing.
        hevent written{true};
        bool wrote = false;
        std::jthread writer([&] {
            wrote = pipe_.write(first_call);
            written.set();
        });

        const bool taken = written.wait_for(std::chrono::seconds(10));

        if (!taken) ::CancelSynchronousIo(writer.native_handle());

        ASSERT_TRUE(taken) << "the first call did not take the whole write";
        EXPECT_TRUE(wrote);

        // The worker issues the second call right after; a moment for it to get there.
        ::Sleep(50);

#ifndef NDEBUG
        EXPECT_EQ(io_op::debug.chained.load() - chained_before, 1u);
#endif
    }

    done.release();

    EXPECT_TRUE(std::all_of(buffer.begin(), buffer.begin() + io_op::call_size,
                            [](std::byte b) { return b == std::byte{5}; }));
    EXPECT_EQ(sta_loop::run_pending(), 0u) << "a given-up operation resumed somebody";
}
