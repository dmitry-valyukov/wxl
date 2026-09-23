module;

#include "abi.h"

export module wxl.async:async_directory;

import :awaitable;
import :sta_loop;
import wxl.core;
import std;

export namespace wxl::async {

/// A directory whose every call happens on the loop's worker thread.
///
/// The same split as `async_file`: `core::directory` knows how to talk to
/// Windows, this one knows where that talking runs and that a failure has to
/// reach a co_await as an exception. Which loop is neither asked nor told --
/// see `sta_loop::instance()`.
///
/// The same split between what may be left to finish alone and what is waited
/// for: opening, exists(), create_all() and remove() own what they touch and are
/// orphanable; next() and close() borrow this object.
///
/// A listing is worth having asynchronous even more than a file is. A directory
/// on a network share, or one with tens of thousands of names in it, keeps
/// `FindNextFileW` busy for as long as it likes, and a reader that watches
/// folders for books does this while somebody is reading.
class async_directory
{
public:
    using entry = core::directory::entry;

    async_directory() = default;

    async_directory(async_directory&&) noexcept = default;
    async_directory& operator=(async_directory&&) noexcept = default;

    /// Starts an enumeration.
    ///
    /// \param pattern the directory and the mask together, as `core::directory`
    ///        wants them: `dir / L"*.fb3"`. Copied into the operation, for the
    ///        reason `async_file` gives at length -- a borrowed path outlives
    ///        nothing once the statement that started the coroutine has ended,
    ///        and the worker is already holding it by then.
    /// \throw system_exception at the co_await if there is nothing to enumerate.
    static awaitable<async_directory> open(const core::path& pattern);

    /// The next name, or nothing when the listing is over.
    ///
    /// The name inside is a view into this object -- which lives in the
    /// coroutine frame -- and it is good until the next co_await of next(), the
    /// same rule the synchronous listing has.
    [[nodiscard]] awaitable<std::optional<entry>> next();

    /// Closes on the worker thread. Not required -- the object closes itself
    /// when it dies -- but that death happens on the STA thread.
    [[nodiscard]] awaitable<void> close();

    inline bool opened() const noexcept { return directory_.opened(); }

    /// \return whether there is a directory at this path.
    [[nodiscard]] static awaitable<bool> exists(const core::path& path);

    /// Makes the whole chain of directories.
    ///
    /// The copy every operation here makes is load-bearing in this one:
    /// `core::directory::create_all()` walks a *writable* path, cutting it short
    /// at each separator in turn, and what it cuts is the operation's own copy
    /// rather than anything the caller can see.
    ///
    /// \throw system_exception at the co_await if the chain could not be made.
    [[nodiscard]] static awaitable<void> create_all(const core::path& p);

    /// Removes an empty directory.
    /// \throw system_exception at the co_await if it could not be removed.
    [[nodiscard]] static awaitable<void> remove(const core::path& path);

private:
    inline explicit async_directory(core::directory&& opened) : directory_(std::move(opened)) {}

    core::directory directory_;
};

}  // export namespace wxl::async
