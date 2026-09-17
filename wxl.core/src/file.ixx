module;

#include "platform.h"

export module wxl.core:file;

import :compressed_optional;
import std;

export namespace wxl::core {

/// An open file: one handle, closed however the object leaves.
///
/// Deliberately thin -- what it adds to `CreateFileW` is ownership of the
/// handle, the loop `ReadFile` needs to be given a whole file rather than two
/// gigabytes of it, and a name for each of the two ways a file gets opened.
/// What it does not add is a policy: nothing here throws, nothing here logs,
/// and nothing here decides what a failure means. That decision belongs to the
/// caller, and in this tree the caller is `wxl::async::async_file`, which turns
/// a failure into the exception its coroutine catches.
///
/// The two ways of opening are two named functions rather than one function
/// with a flag, because reading and writing differ in more than a bit: what is
/// shared, what happens to a file that is already there, and what the caller is
/// then allowed to do with the handle.
///
/// It holds no memory of its own, so it is the one part of an asynchronous
/// operation that may cross to the worker thread and back freely.
class file
{
public:
    /// A closed file. Every operation on it fails the way an operation on a
    /// file that could not be opened fails, so a caller that means to check has
    /// exactly one thing to check.
    file() = default;

    inline file(file&& other) noexcept
        : handle_(std::exchange(other.handle_, INVALID_HANDLE_VALUE)) {}

    inline file& operator=(file&& other) noexcept {
        std::swap(handle_, other.handle_);
        return *this;
    }

    file(const file&) = delete;
    file& operator=(const file&) = delete;

    inline ~file() { close(); }

    /// Opens an existing file for reading.
    ///
    /// Shared for writing as well as for reading: a file held open by an editor
    /// is no reason to refuse to read it.
    ///
    /// \param path the characters themselves, null-terminated -- this class does
    ///        not take a `wxl::core::path`, and not for lack of one. A path
    ///        keeps its characters in the STA pool, and building one here would
    ///        mean building it wherever this call happens to run, which for an
    ///        asynchronous file is the worker thread. The caller passes c_str()
    ///        of a path it already has, on the thread where that path lives.
    /// \return the file, opened or not -- ask opened(), and ask the system
    ///         (`GetLastError`) why not, before anything else is called.
    inline static file open_read(const wchar_t* path) noexcept {
        return file(::CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    }

    /// Creates a file for writing, emptying one that is already there.
    ///
    /// Shared with nobody: what is being written is not worth reading until it
    /// is whole.
    ///
    /// \param path as in open_read(): the characters, not a `path`.
    inline static file create(const wchar_t* path) noexcept {
        return file(::CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr));
    }

    inline bool opened() const noexcept { return handle_ != INVALID_HANDLE_VALUE; }

    /// The file's length, or none when it cannot be had.
    inline nullable<std::uint64_t> size() const noexcept {
        LARGE_INTEGER length{};

        if (!opened() || !::GetFileSizeEx(handle_, &length)) return {};

        return static_cast<std::uint64_t>(length.QuadPart);
    }

    /// Reads into the buffer and answers with how much it got: less than asked
    /// for at the end of the file, and nothing at all on an error -- which of
    /// the two it was, the system knows.
    ///
    /// It loops because one `ReadFile` is bounded by what a DWORD holds and a
    /// file is not.
    inline std::size_t read(std::span<std::byte> into) noexcept {
        std::size_t done = 0;

        while (opened() && done != into.size()) {
            const DWORD wanted =
                static_cast<DWORD>(std::min<std::size_t>(into.size() - done, chunk_limit));
            DWORD got = 0;

            if (!::ReadFile(handle_, into.data() + done, wanted, &got, nullptr) || got == 0) break;

            done += got;
        }

        return done;
    }

    /// Writes the whole buffer, in the same chunks and for the same reason.
    /// \return how much went out; less than asked for means the system refused.
    inline std::size_t write(std::span<const std::byte> from) noexcept {
        std::size_t done = 0;

        while (opened() && done != from.size()) {
            const DWORD wanted =
                static_cast<DWORD>(std::min<std::size_t>(from.size() - done, chunk_limit));
            DWORD written = 0;

            // A write can succeed and take less than it was given -- a full disk
            // is where that happens -- and one that took nothing would leave
            // this loop going round for ever, silently, on a worker thread. So
            // "took nothing" ends it exactly as a refusal does.
            if (!::WriteFile(handle_, from.data() + done, wanted, &written, nullptr) ||
                written == 0)
                break;

            done += written;
        }

        return done;
    }

    /// Pushes what has been written all the way to the device.
    ///
    /// Not a nicety: a file that is about to be renamed over another one has to
    /// be on the disk first, or a power cut leaves the rename done and the
    /// content not.
    inline bool flush() noexcept { return opened() && ::FlushFileBuffers(handle_) != 0; }

    inline void close() noexcept {
        if (opened()) ::CloseHandle(std::exchange(handle_, INVALID_HANDLE_VALUE));
    }

    /// For the calls this class does not wrap -- and it wraps only what this
    /// tree asks for.
    inline HANDLE native_handle() const noexcept { return handle_; }

private:
    inline explicit file(HANDLE handle) noexcept : handle_(handle) {}

    /// A gigabyte: comfortably inside a DWORD, and round.
    static constexpr std::size_t chunk_limit = 0x4000'0000;

    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

}  // export namespace wxl::core
