module;

#include "abi.h"

export module wxl.async:async_file;

import :awaitable;
import :sta_loop;
import wxl.core;
import std;

export namespace wxl::async {

/// A file whose every blocking call happens on the loop's worker thread, and
/// whose every answer comes back to the coroutine that asked.
///
/// It is `core::file` and nothing else: the handle, the reads and the writes
/// are that class's, and what is added here is only where they run. Which is
/// the whole point of the split -- the synchronous file knows how to talk to
/// Windows and owes nobody an explanation, and this one knows about threads and
/// about the fact that a failure has to reach a `co_await` as an exception.
///
/// **Failures become exceptions here**, thrown on the worker thread, carried
/// back inside the operation and rethrown by the co_await. `system_exception`
/// is constructed the moment the call fails, so the code it keeps is the one
/// the failed call left behind.
///
/// **Which loop is not asked and cannot be told**: there is one, it is the STA
/// thread's, and `sta_loop::instance()` is where these calls find it.
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
    async_file() = default;

    async_file(async_file&&) noexcept = default;
    async_file& operator=(async_file&&) noexcept = default;

    /// Opens an existing file for reading.
    /// \throw system_exception at the co_await if it could not be opened.
    static awaitable<async_file> open_read(const core::path& path);

    /// Creates a file for writing, emptying one that is already there.
    /// \throw system_exception at the co_await if it could not be created.
    static awaitable<async_file> create(const core::path& path);

    /// Reads into the caller's buffer -- which lives in the coroutine frame and
    /// is therefore still there when the worker gets to it.
    /// \return how much was read; zero at the end of the file.
    [[nodiscard]] awaitable<std::size_t> read(std::span<std::byte> into);

    /// \throw system_exception if less went out than was asked for.
    [[nodiscard]] awaitable<std::size_t> write(std::span<const std::byte> from);

    /// \throw system_exception if the file's length could not be had.
    [[nodiscard]] awaitable<std::uint64_t> size();

    /// \throw system_exception if the flush failed -- and a failed flush is the
    ///        difference between a file that survives a power cut and one that
    ///        does not, so it is not a failure to pass over.
    [[nodiscard]] awaitable<void> flush();

    /// Closes on the worker thread, where the closing belongs.
    ///
    /// Not required: an async_file left alone closes itself when it is
    /// destroyed. But that happens wherever the object dies, which is the STA
    /// thread, and `CloseHandle` on a file that has been written is not always
    /// quick. So a coroutine that has just written something says so here.
    [[nodiscard]] awaitable<void> close();

    inline bool opened() const noexcept { return file_.opened(); }

private:
    inline explicit async_file(core::file&& opened) : file_(std::move(opened)) {}

    core::file file_;
};

}  // export namespace wxl::async
