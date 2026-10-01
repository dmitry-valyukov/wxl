module;

#include "abi.h"

// A view of text whose next unit is a zero, and says so in the type.
//
// A string handed to a C API or to WinRT wants that zero: HSTRING is a
// pointer and a length with a terminator after them, and a fast-pass string
// (a header on the stack over somebody's characters) is only legal when the
// terminator is there. std::basic_string_view promises nothing about the unit
// after its end, so a function taking one and passing it on has either to copy
// the text or to trust a comment. This is the third answer: the promise is the
// type, and whoever wants the zero asks for it in the signature.
//
// Four doors in, and the first three cost nothing. A C string is terminated by
// definition, and so is anything that ends where its length says and holds a
// zero after it -- std::basic_string, and checked text that owns its buffer
// (u16_text converts on its own, see unicode.ixx), and an hstring, which is
// terminated by construction. The fourth door is assume_terminated(): for a view
// somebody else says is terminated -- an arena that writes one unit more after
// every string -- and it is the only one that has to look, because it is the
// only one that takes the promise on trust.
//
// The unit is char16_t in wxl, since that is what a string is here (wxl::wstring
// is one), and what an HSTRING holds; wchar_t enters through the same seam as
// everywhere else and leaves as wide() / wc_str().

export module wxl.core:zstring_view;

import :checks;
import std;

namespace wxl::core {

namespace impl {

/// The key to the constructor that trusts its caller, deliberately not exported:
/// a promise that anyone can make in passing is a comment with extra syntax. The
/// doors that are allowed to make it are in this file, and outside it there is
/// no way to name the key.
struct terminated {
    explicit terminated() = default;
};

/// What an empty view points at, so that data() of every view is a real
/// terminated string -- a null pointer would make the zero a promise about
/// nothing.
template <typename CharT>
inline constexpr CharT empty_text[1] = {};

}  // namespace impl

export {

template <typename CharT, typename Traits = std::char_traits<CharT>>
class basic_zstring_view {
public:
    using value_type = CharT;
    using traits_type = Traits;
    using size_type = std::size_t;
    using const_pointer = const CharT*;
    using const_iterator = const CharT*;
    using view_type = std::basic_string_view<CharT, Traits>;

    static constexpr size_type npos = view_type::npos;

    /// Empty, and terminated like everything else here.
    constexpr basic_zstring_view() noexcept = default;

    /// A C string: the length is where the zero is.
    constexpr basic_zstring_view(const_pointer text) noexcept
        : ptr_(text), size_(length_of(text)) {}

    /// A string, whatever allocator it carries: std::basic_string keeps the zero.
    template <typename Alloc>
    constexpr basic_zstring_view(const std::basic_string<CharT, Traits, Alloc>& text) noexcept
        : ptr_(text.c_str()), size_(text.size()) {}

    // The seam of 0295: text that is still written in wchar_t. The units are the
    // same sixteen bits on Windows under another name, and the cast is the whole
    // conversion.
    basic_zstring_view(const wchar_t* text) noexcept
        requires std::same_as<CharT, char16_t>
        : basic_zstring_view(reinterpret_cast<const char16_t*>(text)) {}

    template <typename Alloc>
    basic_zstring_view(const std::basic_string<wchar_t, std::char_traits<wchar_t>, Alloc>& text) noexcept
        requires std::same_as<CharT, char16_t>
        : ptr_(reinterpret_cast<const char16_t*>(text.c_str())), size_(text.size()) {}

    /// The terminated wchar_t view as the char16_t one, with the terminator it
    /// carries: the same seam, one level up.
    template <typename Unit>
        requires(std::same_as<CharT, char16_t> && std::same_as<Unit, wchar_t>)
    basic_zstring_view(basic_zstring_view<Unit> other) noexcept
        : ptr_(reinterpret_cast<const char16_t*>(other.data())), size_(other.size()) {}

    constexpr const_pointer data() const noexcept { return ptr_; }
    constexpr const_pointer c_str() const noexcept { return ptr_; }
    constexpr size_type size() const noexcept { return size_; }
    constexpr size_type length() const noexcept { return size_; }
    constexpr bool empty() const noexcept { return size_ == 0; }

    constexpr const_iterator begin() const noexcept { return ptr_; }
    constexpr const_iterator end() const noexcept { return ptr_ + size_; }
    constexpr CharT operator[](size_type at) const noexcept { return ptr_[at]; }
    constexpr CharT front() const noexcept { return ptr_[0]; }
    constexpr CharT back() const noexcept { return ptr_[size_ - 1]; }

    /// Terminated, so the pointer alone is enough for a callee that wants one,
    /// in the unit the callee spells.
    const wchar_t* wc_str() const noexcept
        requires std::same_as<CharT, char16_t>
    {
        return reinterpret_cast<const wchar_t*>(ptr_);
    }

    /// The same text as a wstring_view -- what the code that still says wchar_t
    /// takes. It is the view only, with no terminator promised on the way out.
    std::wstring_view wide() const noexcept
        requires std::same_as<CharT, char16_t>
    {
        return {reinterpret_cast<const wchar_t*>(ptr_), size_};
    }

    /// The text without the promise, for the code that wants a view.
    constexpr operator view_type() const noexcept { return view_type(ptr_, size_); }

    constexpr view_type view() const noexcept { return view_type(ptr_, size_); }

    /// From `at` to the end -- the one slice that keeps the terminator, and so
    /// stays a zstring_view. A slice with a count ends before the zero and is
    /// a plain view; that is why it does not answer with this type.
    constexpr basic_zstring_view substr(size_type at) const noexcept {
        ensure(at <= size_ && "substr past the end");
        return basic_zstring_view(impl::terminated{}, ptr_ + at, size_ - at);
    }

    constexpr view_type substr(size_type at, size_type count) const noexcept {
        ensure(at <= size_ && "substr past the end");
        return view_type(ptr_ + at, std::min(count, size_ - at));
    }

    /// Cuts from the front: the end, and the zero after it, stay where they were.
    /// There is no remove_suffix -- it would leave a view that ends before its
    /// terminator.
    constexpr void remove_prefix(size_type count) noexcept {
        ensure(count <= size_ && "remove_prefix past the end");
        ptr_ += count;
        size_ -= count;
    }

    friend constexpr bool operator==(basic_zstring_view left, basic_zstring_view right) noexcept {
        return left.view() == right.view();
    }

    friend constexpr bool operator==(basic_zstring_view left, view_type right) noexcept {
        return left.view() == right;
    }

    /// A literal against a view: without this the array would reach either of
    /// the two above through a conversion each, and neither would be better.
    friend constexpr bool operator==(basic_zstring_view left, const_pointer right) noexcept {
        return left.view() == view_type(right);
    }

    friend constexpr auto operator<=>(basic_zstring_view left, basic_zstring_view right) noexcept {
        return left.view() <=> right.view();
    }

    /// The doors that have looked, or need not: the promise is theirs, and the
    /// key is what keeps it from being made anywhere else.
    constexpr basic_zstring_view(impl::terminated, const_pointer text, size_type size) noexcept
        : ptr_(text), size_(size) {}

private:
    static constexpr size_type length_of(const_pointer text) noexcept {
        ensure(text != nullptr && "a zstring_view cannot be made from a null pointer");
        return Traits::length(text);
    }

    const_pointer ptr_ = impl::empty_text<CharT>;
    size_type size_ = 0;
};

/// UTF-16, the string of this tree: what HSTRING holds and what `wxl::wstring`
/// is a string of.
using zstring_view = basic_zstring_view<char16_t>;

/// A view somebody else says is terminated: an arena that writes one unit past
/// every string it hands out, a buffer whose owner zeroes the end. The one door
/// that takes the promise on trust, and named so that a reader greps for it on
/// the day the promise turns out to be false.
///
/// It looks at the unit after the view, in every build. That is a tripwire and
/// not a proof: when the promise holds the unit is inside the string and is
/// zero, and when it does not the read is the very thing that is wrong -- one
/// past the end of a buffer. It is here for the same reason cppwinrt's
/// create_hstring_on_stack has it: the failure is loud where it is caused, not
/// silent where it is used. Every other door pays nothing.
template <typename CharT, typename Traits>
constexpr basic_zstring_view<CharT, Traits> assume_terminated(
    std::basic_string_view<CharT, Traits> text) noexcept {
    if (text.data() == nullptr) {
        ensure(text.empty() && "a view over nothing has to be empty");
        return {};
    }

    ensure(text.data()[text.size()] == CharT{} && "assume_terminated on a view with no zero after it");
    return basic_zstring_view<CharT, Traits>(impl::terminated{}, text.data(), text.size());
}

/// The seam: a wstring_view that is terminated, as a zstring_view.
inline zstring_view assume_terminated(std::wstring_view text) noexcept {
    return assume_terminated(
        std::u16string_view(reinterpret_cast<const char16_t*>(text.data()), text.size()));
}

}  // export

}  // namespace wxl::core
