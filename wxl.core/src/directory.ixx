module;

#include "platform.h"

export module wxl.core:directory;

import :path;
import std;

export namespace wxl::core {

/// A directory: what is in it, and the four things one does to it.
///
/// Two halves that happen to belong to the same subject. An *object* -- an open
/// enumeration, which is a handle and one entry at a time -- and a few *static*
/// operations that take a path and do not need anything kept open: does it
/// exist, make it, make the whole chain of it, remove it.
///
/// Like `file`, it has no memory of its own and no policy: nothing here throws
/// and nothing decides what a failure means. And, like `file`, it takes paths as
/// characters rather than as `path`, because a `path` built here would be built
/// on whichever thread the call runs on -- the worker thread, for the
/// asynchronous form -- and its characters belong to the pool of another.
class directory
{
public:
    /// One name out of a directory.
    ///
    /// The name is a view into this object's own buffer and lives until the next
    /// next() -- or until the listing is moved, which takes the buffer with it.
    /// The enumeration reuses one buffer, and a caller that wants to keep a name
    /// copies it. That is what makes walking a directory cost no allocations at
    /// all.
    struct entry {
        std::wstring_view name;
        bool is_directory = false;
        std::uint64_t size = 0;
    };

    directory() = default;

    inline directory(directory&& other) noexcept
        : handle_(std::exchange(other.handle_, INVALID_HANDLE_VALUE)),
          found_(other.found_),
          pending_(std::exchange(other.pending_, false)) {}

    inline directory& operator=(directory&& other) noexcept {
        std::swap(handle_, other.handle_);
        std::swap(found_, other.found_);
        std::swap(pending_, other.pending_);
        return *this;
    }

    directory(const directory&) = delete;
    directory& operator=(const directory&) = delete;

    inline ~directory() { close(); }

    /// Starts an enumeration.
    ///
    /// \param pattern the directory and what to match in it, together, the way
    ///        Windows wants them -- `C:\\books\\*` for everything, or
    ///        `C:\\books\\*.fb3` for the only files a reader cares about.
    ///        Filtering by the system beats filtering by us: the names we never
    ///        see cost nothing.
    /// \return the enumeration, started or not -- ask opened().
    inline static directory open(const wchar_t* pattern) noexcept {
        directory result;

        // The newer of the two: FindExInfoBasic asks the system not to look up
        // the 8.3 name nobody here wants, and LARGE_FETCH lets it hand over more
        // than one entry per trip into the kernel.
        result.handle_ = ::FindFirstFileExW(pattern, FindExInfoBasic, &result.found_,
                                            FindExSearchNameMatch, nullptr,
                                            FIND_FIRST_EX_LARGE_FETCH);

        result.pending_ = result.opened();

        return result;
    }

    inline bool opened() const noexcept { return handle_ != INVALID_HANDLE_VALUE; }

    /// The next name, or `false` when there are none left.
    ///
    /// "." and ".." never come out: every caller of a directory listing has to
    /// drop them, and a listing that hands them over only makes each caller
    /// remember the same two names.
    inline bool next(entry& out) noexcept {
        while (opened()) {
            if (!std::exchange(pending_, false) && !::FindNextFileW(handle_, &found_)) return false;

            const std::wstring_view name = found_.cFileName;

            if (name == L"." || name == L"..") continue;

            out.name = name;
            out.is_directory = (found_.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            out.size = (static_cast<std::uint64_t>(found_.nFileSizeHigh) << 32) | found_.nFileSizeLow;

            return true;
        }

        return false;
    }

    inline void close() noexcept {
        if (opened()) ::FindClose(std::exchange(handle_, INVALID_HANDLE_VALUE));

        pending_ = false;
    }

    /// \return `true` if there is a directory at this path. A file there is not
    ///         a directory, and answers `false`.
    inline static bool exists(const wchar_t* path) noexcept {
        const DWORD attributes = ::GetFileAttributesW(path);

        return attributes != INVALID_FILE_ATTRIBUTES &&
               (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    /// Makes one directory, whose parent has to be there already.
    ///
    /// \return `true` if the directory is there afterwards -- which includes its
    ///         having been there before. "Make sure it exists" is what every
    ///         caller in this tree means, and a separate answer for "it already
    ///         was" would only be thrown away at each of them.
    inline static bool create(const wchar_t* path) noexcept {
        return ::CreateDirectoryW(path, nullptr) != 0 ||
               ::GetLastError() == ERROR_ALREADY_EXISTS;
    }

    /// Makes the whole chain, parents first.
    ///
    /// \param p writable, and left exactly as it was found: the chain is
    ///        walked by cutting the path short at each separator in turn and
    ///        putting the character back afterwards. That is the whole reason
    ///        this one takes a `path&` rather than characters -- the alternative
    ///        is a fresh string per level, and this runs where allocating is
    ///        least welcome.
    inline static bool create_all(path& p) noexcept {
        wchar_t* const text = p.data();

        if (p.empty()) return false;

        std::size_t at = root_length(p);

        for (; text[at]; ++at) {
            if (!path::is_separator(text[at])) continue;

            const wchar_t separator = std::exchange(text[at], L'\0');
            const bool made = create(text);

            text[at] = separator;

            if (!made) return false;
        }

        return create(text);
    }

    /// Removes an empty directory. A directory with anything in it stays, and
    /// the answer is `false`: emptying it first is a decision, and not this
    /// function's to make.
    inline static bool remove(const wchar_t* path) noexcept {
        return ::RemoveDirectoryW(path) != 0;
    }

private:
    /// How much of the path is root -- the part create_all() must not try to
    /// make, because it is not a directory anybody creates.
    ///
    /// Three shapes. A drive letter and its separator, `C:\`. A separator on its
    /// own, rooted on the current drive. And a UNC name, `\\server\share`, where
    /// the root runs to the end of the share: neither `\\server` nor a bare
    /// `\\server\` is something `CreateDirectoryW` will make, and a books folder
    /// watched on a network share is written exactly this way.
    inline static std::size_t root_length(const path& p) noexcept {
        const std::wstring_view text = p.native();

        if (text.size() >= 2 && text[1] == L':')
            return (text.size() >= 3 && path::is_separator(text[2])) ? 3u : 2u;

        if (text.size() >= 2 && path::is_separator(text[0]) && path::is_separator(text[1])) {
            const std::size_t server = text.find_first_of(L"\\/", 2);

            if (server == std::wstring_view::npos) return text.size();

            const std::size_t share = text.find_first_of(L"\\/", server + 1);

            return share == std::wstring_view::npos ? text.size() : share + 1;
        }

        return path::is_separator(text[0]) ? 1u : 0u;
    }

    HANDLE handle_ = INVALID_HANDLE_VALUE;

    /// The entry the system left here: filled by the open, then by every step.
    WIN32_FIND_DATAW found_{};

    /// FindFirstFileEx already found one, and next() has to hand that one over
    /// before asking for another.
    bool pending_ = false;
};

}  // export namespace wxl::core
