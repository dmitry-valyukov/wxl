// Reads the kernel is holding: what arrives through the port, and what giving such a
// read up does. A pipe nobody writes to stands for the slow device -- a read from it
// stays with the kernel until something is written or the read is cancelled.
#include <gtest/gtest.h>

#include "platform.h"

#include "sta_pool.h"

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

class AsyncFilePipeTest : public ::testing::Test
{
protected:
    void SetUp() override {
        ASSERT_TRUE(pipe_.made());

        root_ = path(std::filesystem::temp_directory_path().wstring()) /
                L"wxl_async_file_pipe_tests";

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

    sta_loop::run_until([&] { return reading; });

    EXPECT_FALSE(work.done());
    ASSERT_TRUE(pipe_.write("hello"));

    sta_loop::run_until([&] { return work.done(); });
    work.result();

    EXPECT_EQ(got, "hello");
}

TEST_F(AsyncFilePipeTest, AFrameUnwindingPastAReadInTheKernelCancelsIt) {
    rescue watchdog(pipe_);

    task work = first_read_fails_second_is_with_the_kernel(root_ / L"refusing.bin", pipe_.name());

    sta_loop::run_until([&] { return work.done(); });

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

        sta_loop::run_until([&] { return reading; });

        EXPECT_FALSE(work.done());
    }

    EXPECT_FALSE(watchdog.was_needed()) << "the read was waited for instead of cancelled";
    EXPECT_EQ(sta_loop::run_pending(), 0u) << "a given-up operation resumed somebody";
}
