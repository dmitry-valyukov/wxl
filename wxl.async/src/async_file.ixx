module;

#include "abi.h"

export module wxl.async:async_file;

import :awaitable;
import :sta_loop;
import wxl.core;
import std;

export namespace wxl::async {

/// A file that never holds the STA thread, and whose every answer comes back to
/// the coroutine that asked.
///
/// The handle is `core::file`'s, opened for overlapped operations and attached
/// to the loop's port. What is added here is where each call runs:
///
/// - **A read starts on the STA thread itself**, which saves the trip to the
///   worker. One the kernel takes comes back through the port; one the system
///   chooses to finish inside the call is over there and then, and the
///   co_await does not suspend.
/// - **What is known to hold the calling thread goes to the worker**: every
///   write, since a file made here starts empty and each write grows it; a read
///   into more than `direct_read_limit` bytes, which served inside the call
///   would copy that much on the calling thread; any read from a compressed or
///   encrypted file; flush().
/// - **size() and close() run on the STA thread**: they take microseconds.
/// - **Opening and creating go by a name**, which the system may take as long
///   as it likes over. Under `sta_loop::start_dispatched()` they are carried out
///   on the system's thread pool and come back through the thread's dispatcher
///   queue; under the other two shapes of the loop, on the worker.
///
/// **The position is kept here**, an overlapped handle having none: it moves on
/// by what was asked for as an operation starts, so that two reads started one
/// after another ask for two different places. A read that brings less has
/// reached the end of the file.
///
/// **Failures become exceptions here**, carried back inside the operation and
/// rethrown by the co_await. `system_exception` keeps the code the failed call
/// left behind.
///
/// **Which loop is not asked and cannot be told**: there is one, it is the STA
/// thread's, and `sta_loop` is static from top to bottom.
///
/// **Giving an operation up.** Opening and creating own everything they touch,
/// so an awaitable that goes away before them leaves them to finish alone
/// (`orphanable`), and the file is let go of at once: one already open is
/// closed before the awaitable's destructor returns, one being opened has its
/// opening cut short, and one not reached yet is never opened. So what handles
/// the failure may ask for the same file straight away.
/// Everything else borrows this object and the
/// caller's buffer, and an awaitable giving one of those up waits until the
/// operation has come back -- which a read or a write the kernel holds is told
/// to do at once, by CancelIoEx.
///
/// **The path is copied into the operation, and that is deliberate.** Borrowing
/// it looks free and is a trap: the operation is handed to the worker inside the
/// call that starts it, and by the time the coroutine's caller has finished the
/// statement -- `co_await open_read(dir / name)` written *around* a coroutine
/// call rather than inside it -- the temporary path is gone while the worker is
/// still holding the pointer. The copy costs one allocation from the STA pool,
/// made and released on the STA thread, which is the very thing
/// `wxl::core::path` exists to make cheap; a use-after-free costs rather more.
class async_file
{
public:
    /// The largest read started on the STA thread. Copying this much takes about what a
    /// trip through the worker does; more goes to the worker.
    static constexpr std::size_t direct_read_limit = 64 * 1024;

    async_file() = default;

    async_file(async_file&&) noexcept = default;
    async_file& operator=(async_file&&) noexcept = default;

    /// Opens an existing file for reading.
    /// \throw system_exception at the co_await if it could not be opened.
    static awaitable<async_file> open_read(const core::path& path);

    /// Creates a file for writing, emptying one that is already there.
    /// \throw system_exception at the co_await if it could not be created.
    static awaitable<async_file> create(const core::path& path);

    /// Reads into the caller's buffer, which has to stay where it is until the
    /// read has been awaited or given up.
    /// \return how much was read; less than asked for at the end of the file,
    ///         and zero past it.
    [[nodiscard]] awaitable<std::size_t> read(std::span<std::byte> into);

    /// The same read into an array of byte-sized elements -- `char buffer[N]`
    /// as readily as `std::byte` -- so a caller whose buffer is text does not
    /// spell `as_writable_bytes` at every call. Any one-byte trivially
    /// copyable type but `bool`, which is a byte that must not hold 2.
    template <class T, std::size_t N>
        requires (sizeof(T) == 1 && std::is_trivially_copyable_v<T> && !std::is_same_v<T, bool>)
    [[nodiscard]] awaitable<std::size_t> read(T (&into)[N]) {
        return read(std::as_writable_bytes(std::span{into}));
    }

    /// \throw system_exception if less went out than was asked for.
    [[nodiscard]] awaitable<std::size_t> write(std::span<const std::byte> from);

    /// \throw system_exception if the file's length could not be had.
    [[nodiscard]] awaitable<std::uint64_t> size();

    /// \throw system_exception if the flush failed -- and a failed flush is the
    ///        difference between a file that survives a power cut and one that
    ///        does not, so it is not a failure to pass over.
    [[nodiscard]] awaitable<void> flush();

    /// Not required: an async_file left alone closes itself when it is
    /// destroyed.
    [[nodiscard]] awaitable<void> close();

    inline bool opened() const noexcept { return file_.opened(); }

private:
    /// Attaches the file to the loop's port and asks what kind it is. Called where
    /// the file was opened, which is not the STA thread.
    explicit async_file(core::file&& opened);

    core::file file_;
    std::uint64_t position_ = 0;

    /// What io_port::attach() answered.
    bool skips_port_ = false;

    /// Compressed or encrypted: its reads are served inside the call that starts them.
    bool reads_hold_the_caller_ = false;
};

}  // export namespace wxl::async
