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
        DWORD written = 0;

        return ::WriteFile(server_, text.data(), static_cast<DWORD>(text.size()), &written,
                           nullptr) &&
               written == text.size();
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

task reads_once(path pipe_name, std::string& got, bool& reading) {
    async_file in = co_await async_file::open_read(pipe_name);

    char buffer[64];

    auto read = in.read(buffer);

    reading = true;
    got.assign(buffer, co_await read);
}

/// The code from the discussion, on handles of its own: the first read fails, and the
/// frame unwinds with the second one in the kernel's hands, waiting to write into
/// `second_buffer`.
task first_read_fails_second_is_with_the_kernel(path refusing, path pipe_name) {
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

task reads_and_is_dropped(path pipe_name, bool& reading) {
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
task reads_among_orphans_given_up(path file_path, int rounds, int& cut_short, int& read) {
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

    task work = reads_once(pipe_.name(), got, reading);

    wait_until([&] { return reading; });

    EXPECT_FALSE(work.done());
    ASSERT_TRUE(pipe_.write("hello"));

    wait_until([&] { return work.done(); });
    work.result();

    EXPECT_EQ(got, "hello");
}

TEST_F(AsyncFilePipeTest, AFrameUnwindingPastAReadInTheKernelCancelsIt) {
    rescue watchdog(pipe_);

    task work = first_read_fails_second_is_with_the_kernel(root_ / L"refusing.bin", pipe_.name());

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
        task work = reads_and_is_dropped(pipe_.name(), reading);

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

    task work = reads_among_orphans_given_up(target, rounds, cut_short, read);

    wait_until([&] { return work.done(); });
    work.result();

    EXPECT_EQ(cut_short, 0);
    EXPECT_EQ(read, rounds);
}
