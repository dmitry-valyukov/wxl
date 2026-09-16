module;

// ensure() is a macro, and its release form names wxl::core::fail -- so the
// header has to be here, in the global fragment, and :checks imported below.
// A translation unit that gets abi.h from a precompiled header would compile
// without this; Bukvitsa builds these sources without one, and that is the
// check that catches it.
#include "abi.h"

export module wxl.core:path;

import :checks;
import :sta_allocator;
import std;

export namespace wxl::core {

/// A filesystem path, kept in the STA pool.
///
/// It exists because `std::filesystem::path` cannot be told where to get its
/// memory: it is a concrete class, not a template, its storage is a
/// `std::wstring` with `std::allocator`, and the only allocator-aware thing
/// about it is the conversion *out* of it. A GUI thread that assembles a path
/// therefore walks into the CRT heap -- a lock and an unpredictable amount of
/// time -- for something it is going to hand to `CreateFileW` and forget.
///
/// So this one is `sta_wstring` and a handful of operations, and the handful is
/// not a first cut: it is what a whole reader actually asks of a path. c_str()
/// for every call into the system, joining a directory with a name, the text
/// itself for storing and showing, and -- one call site each -- filename() and
/// parent_path(). Nothing here normalizes, resolves, compares case-insensitively
/// or walks components, because nothing asked.
///
/// **Wide characters, and not by habit.** Every path in Windows reaches us as
/// UTF-16 (a dialog, a drop, a known folder) and leaves as UTF-16, and it is
/// not obliged to be valid UTF-16 on the way -- an unpaired surrogate in a file
/// name is not forbidden. Storing it as UTF-8 would mean a conversion at every
/// call into the system and a lossy repair at the door.
///
/// **It belongs to the pool's thread**, like everything else built on that
/// pool: made there, grown there, destroyed there. What crosses to another
/// thread is c_str(), and the path has to outlive the call it is passed to.
///
/// **And it wants the pool from the first moment, empty or not.** In a debug
/// build a container allocates as it is constructed -- MSVC's
/// `_Container_proxy`, the bookkeeping behind checked iterators, comes out of
/// the container's own allocator -- so even a default-constructed path reaches
/// into the pool. An object with a path member therefore cannot be built before
/// the pool is, which is a thing to know when the object is a global, or a test
/// fixture whose members are built before its SetUp() runs.
class path
{
public:
    using string_type = sta_wstring;
    using view_type = std::wstring_view;

    path() = default;

    path(view_type text) : text_(text) {}

    path(const wchar_t* text) : text_(text) {}

    /// What the system is called with. Null-terminated, which is why this class
    /// keeps a string at all rather than a view.
    const wchar_t* c_str() const noexcept { return text_.c_str(); }

    /// The text itself -- to store, to show, to put in a message.
    view_type native() const noexcept { return text_; }

    /// The characters, writable, still null-terminated.
    ///
    /// For the one thing that cannot be done to a path from the outside without
    /// them: walking it and cutting it short at each separator in turn, which is
    /// how a chain of directories gets created without allocating a string per
    /// level. See `directory::create_all()`, which puts back what it cut.
    wchar_t* data() noexcept { return text_.data(); }

    bool empty() const noexcept { return text_.empty(); }

    std::size_t size() const noexcept { return text_.size(); }

    /// Appends one component, with a separator where one is needed.
    ///
    /// A component, not a path: appending something rooted -- beginning with a
    /// separator, or with a drive letter and a colon -- is a mistake in the
    /// program rather than a case to be silently reinterpreted, so it is refused
    /// here instead of being turned into whatever `std::filesystem::path` would
    /// have made of it (which is: the rooted side wins and everything to the
    /// left of it is dropped).
    ///
    /// An empty component leaves the path as it is. That is the one place this
    /// differs from `std::filesystem::path`, which appends a separator and
    /// leaves a path ending in nothing -- a name a program then has to check
    /// for. Here "nothing to add" adds nothing.
    path& operator/=(view_type component) {
        if (component.empty()) return *this;

        ensure(!is_rooted(component) && "path: only a relative component can be appended");

        const bool separator_needed = !text_.empty() && !is_separator(text_.back());

        text_.reserve(text_.size() + component.size() + (separator_needed ? 1 : 0));

        if (separator_needed) text_ += preferred_separator;

        text_.append(component);

        return *this;
    }

    friend path operator/(path left, view_type right) {
        left /= right;
        return left;
    }

    /// The last component, or the whole path when there is no separator in it.
    view_type filename() const noexcept {
        const std::size_t separator = last_separator();

        return separator == view_type::npos ? view_type(text_)
                                            : view_type(text_).substr(separator + 1);
    }

    /// Everything before the last component, empty when there is nothing before
    /// it. A separator that is the root -- the first character, or the one after
    /// a drive letter -- stays in, because dropping it would turn an absolute
    /// path into a relative one.
    view_type parent_path() const noexcept {
        const std::size_t separator = last_separator();

        if (separator == view_type::npos) return {};

        const bool separator_is_root = separator == 0 || (separator == 2 && text_[1] == L':');

        return view_type(text_).substr(0, separator_is_root ? separator + 1 : separator);
    }

    /// Ordinal, character by character: no case folding and no normalization.
    /// Windows compares file names case-insensitively, and a program that needs
    /// that asks the system (`CompareStringOrdinal`) rather than this class,
    /// which has no business knowing the rules of a particular volume.
    friend bool operator==(const path& left, const path& right) noexcept {
        return left.text_ == right.text_;
    }

    static constexpr wchar_t preferred_separator = L'\\';

    static constexpr bool is_separator(wchar_t c) noexcept { return c == L'\\' || c == L'/'; }

private:
    /// Rooted: starting at a root rather than somewhere below the path it would
    /// be appended to. Both spellings Windows accepts -- a leading separator,
    /// and a drive letter with a colon.
    static constexpr bool is_rooted(view_type text) noexcept {
        return !text.empty() &&
               (is_separator(text.front()) || (text.size() >= 2 && text[1] == L':'));
    }

    std::size_t last_separator() const noexcept { return view_type(text_).find_last_of(L"\\/"); }

    string_type text_;
};

}  // export namespace wxl::core
