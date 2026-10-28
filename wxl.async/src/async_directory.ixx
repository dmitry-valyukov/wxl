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
/// of them but close(): asked, an operation not started is never started, and one standing
/// in the system has the synchronous call it stands in cancelled where the system can cancel
/// it; list() and create_all() stop between their steps. The cancellation is the answer of
/// one aborted or never started -- a listing stopped halfway is no answer -- and one that
/// got there first answers with what it made; the co_await ends once the operation has let
/// go, of the frame and of the disk, as `async_file`'s do.
///
/// A listing is worth having asynchronous even more than a file is. A directory
/// on a network share, or one with tens of thousands of names in it, keeps
/// `FindNextFileW` busy for as long as it likes, and an application that watches
/// folders for new files does this while somebody is working in its window.
class async_directory
{
public:
    /// The type of one entry of the enumeration: `wxl::core::directory::entry`, whose name is
    /// a view into the listing.
    using entry = core::directory::entry;

    /// What list() brings back: an `entry` that owns its name, since the listing
    /// it was read from is gone by the time anybody looks.
    struct listed_entry {
        std::wstring name;                                 ///< The name of the file or directory.
        bool is_directory = false;                         ///< Whether the entry is a directory.
        std::uint64_t size = 0;                            ///< The file size, in bytes.
        std::chrono::file_clock::time_point last_write{};  ///< The time of the last write.
    };

    async_directory() = default;

    /// Constructs the object by moving the enumeration of another.
    async_directory(async_directory&&) noexcept = default;
    /// Moves the enumeration of another object into this one.
    async_directory& operator=(async_directory&&) noexcept = default;

    /// Starts an enumeration.
    ///
    /// \param pattern the directory and the mask together, as `core::directory`
    ///        wants them: `dir / L"*.txt"`. Copied into the operation, for the
    ///        reason `async_file` gives at length -- a borrowed path outlives
    ///        nothing once the statement that started the coroutine has ended,
    ///        and the worker is already holding it by then.
    /// \throw last_error at the co_await if there is nothing to enumerate.
    static task<async_directory> open(const core::path& pattern);

    /// Everything the pattern matches, at once, in one operation that owns its
    /// result -- orphanable, one round trip, for the folder somebody adds to those
    /// an application watches. The enumeration above is for the other case, the
    /// directory with tens of thousands of names, where one trip per name is the point.
    ///
    /// A pattern that matches nothing is an empty answer, not a failure: the
    /// directory is there and holds none of what was asked for.
    ///
    /// \throw last_error at the co_await if there is no such directory,
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

    /// Tests whether the object holds an open enumeration.
    inline bool opened() const noexcept { return directory_.opened(); }

    /// \return whether there is a directory at this path.
    [[nodiscard]] static task<bool> exists(const core::path& path);

    /// Makes the whole chain of directories.
    ///
    /// The copy every operation here makes is load-bearing in this one:
    /// `core::directory::create_all()` walks a *writable* path, truncating it
    /// at each separator in turn, and what it truncates is the operation's own copy
    /// rather than anything the caller can see.
    ///
    /// \throw last_error at the co_await if the chain could not be made.
    [[nodiscard]] static task<> create_all(const core::path& p);

    /// Removes an empty directory.
    /// \throw last_error at the co_await if it could not be removed.
    [[nodiscard]] static task<> remove(const core::path& path);

    /// The operations above under a token, its last argument: the operation is registered
    /// with the token while it lives, and under a token cancelled already nothing is started.
    /// Each returns the same task as its form without a token.
    ///@{
    static task<async_directory> open(const core::path& pattern, cancellation_token token);

    /// Lists everything that \p pattern matches; the operation can be canceled through \p token.
    [[nodiscard]] static task<std::vector<listed_entry>> list(const core::path& pattern,
                                                              cancellation_token token);

    /// Returns the next entry, or `std::nullopt` when the enumeration is over; the operation can
    /// be canceled through \p token.
    [[nodiscard]] task<std::optional<entry>> next(cancellation_token token);

    /// Tests whether a directory exists at \p path; the operation can be canceled through
    /// \p token.
    [[nodiscard]] static task<bool> exists(const core::path& path, cancellation_token token);

    /// Creates the whole chain of directories of \p p; the operation can be canceled through
    /// \p token.
    [[nodiscard]] static task<> create_all(const core::path& p, cancellation_token token);

    /// Removes the empty directory at \p path; the operation can be canceled through \p token.
    [[nodiscard]] static task<> remove(const core::path& path, cancellation_token token);
    ///@}

private:
    inline explicit async_directory(core::directory&& opened) : directory_(std::move(opened)) {}

    /// The bodies the two forms of open() and next() share (async_directory.cpp).
    static auto opening(const core::path& pattern);
    auto next_entry();

    core::directory directory_;
};

}  // export namespace wxl::async
