module;

export module wxl.async:async_directory;

import :cancellation;
import :sta_loop;
import :task;
import wxl.core;
import std;

export namespace wxl::async {

/// A directory whose every call happens on the loop's worker thread.
///
/// The same split as `async_file`: `core::directory` knows how to talk to
/// Windows, this one knows where that talking runs and that a failure has to
/// reach a co_await as an exception. Which loop is neither asked nor told:
/// there is one, and `sta_loop` is static.
///
/// The same split between what may be left to finish alone and what is waited
/// for: opening, list(), exists(), create_all() and remove() own what they touch
/// and are orphanable; next() and close() borrow this object.
///
/// And the same second form under a `cancellation_token`, its last argument, for all
/// of them but close(): an operation not started is never started, one standing in the
/// system is cut short where the system allows, and the co_await ends with
/// operation_canceled_exception once nothing writes into the frame any more.
///
/// A listing is worth having asynchronous even more than a file is. A directory
/// on a network share, or one with tens of thousands of names in it, keeps
/// `FindNextFileW` busy for as long as it likes, and a reader that watches
/// folders for books does this while somebody is reading.
class async_directory
{
public:
    using entry = core::directory::entry;

    /// What list() brings back: an `entry` that owns its name, since the listing
    /// it was read from is gone by the time anybody looks.
    struct listed_entry {
        std::wstring name;
        bool is_directory = false;
        std::uint64_t size = 0;
    };

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
    static task<async_directory> open(const core::path& pattern);

    /// Everything the pattern matches, at once, in one operation that owns its
    /// result -- orphanable, one round trip, for the folder a reader adds to its
    /// shelf. The enumeration above is for the other case, the directory with
    /// tens of thousands of names, where one trip per name is the point.
    ///
    /// A pattern that matches nothing is an empty answer, not a failure: the
    /// directory is there and holds none of what was asked for.
    ///
    /// \throw system_exception at the co_await if there is no such directory,
    ///        or it could not be read.
    [[nodiscard]] static task<std::vector<listed_entry>> list(const core::path& pattern);

    /// The next name, or nothing when the listing is over.
    ///
    /// The name inside is a view into this object -- which lives in the
    /// coroutine frame -- and it is good until the next co_await of next(), the
    /// same rule the synchronous listing has.
    [[nodiscard]] task<std::optional<entry>> next();

    /// Closes on the worker thread. Not required -- the object closes itself
    /// when it dies -- but that death happens on the STA thread.
    [[nodiscard]] task<> close();

    inline bool opened() const noexcept { return directory_.opened(); }

    /// \return whether there is a directory at this path.
    [[nodiscard]] static task<bool> exists(const core::path& path);

    /// Makes the whole chain of directories.
    ///
    /// The copy every operation here makes is load-bearing in this one:
    /// `core::directory::create_all()` walks a *writable* path, cutting it short
    /// at each separator in turn, and what it cuts is the operation's own copy
    /// rather than anything the caller can see.
    ///
    /// \throw system_exception at the co_await if the chain could not be made.
    [[nodiscard]] static task<> create_all(const core::path& p);

    /// Removes an empty directory.
    /// \throw system_exception at the co_await if it could not be removed.
    [[nodiscard]] static task<> remove(const core::path& path);

    /// The operations above under a token, its last argument.
    ///@{
    static cancellable_task<async_directory> open(const core::path& pattern, cancellation_token stop) {
        return cancellable_task<async_directory>(std::move(stop), [&] { return open(pattern); });
    }

    static cancellable_task<std::vector<listed_entry>> list(const core::path& pattern,
                                                                 cancellation_token stop) {
        return cancellable_task<std::vector<listed_entry>>(std::move(stop), [&] { return list(pattern); });
    }

    cancellable_task<std::optional<entry>> next(cancellation_token stop) {
        return cancellable_task<std::optional<entry>>(std::move(stop), [&] { return next(); });
    }

    static cancellable_task<bool> exists(const core::path& path, cancellation_token stop) {
        return cancellable_task<bool>(std::move(stop), [&] { return exists(path); });
    }

    static cancellable_task<void> create_all(const core::path& p, cancellation_token stop) {
        return cancellable_task<void>(std::move(stop), [&] { return create_all(p); });
    }

    static cancellable_task<void> remove(const core::path& path, cancellation_token stop) {
        return cancellable_task<void>(std::move(stop), [&] { return remove(path); });
    }
    ///@}

private:
    inline explicit async_directory(core::directory&& opened) : directory_(std::move(opened)) {}

    core::directory directory_;
};

}  // export namespace wxl::async
