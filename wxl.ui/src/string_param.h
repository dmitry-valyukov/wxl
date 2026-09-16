#pragma once

// What a string parameter takes, and the one place the two spellings of
// UTF-16 meet.
//
// wxl reads a string out of WinRT as wxl::wstring, which is a string of
// char16_t -- the unit an HSTRING is made of, and the one the standard fixes
// at sixteen bits everywhere. wchar_t is not that: MSVC makes it sixteen
// bits, gcc and clang make it thirty-two, so text written in wchar_t is
// UTF-16 only by a coincidence of platform. Code here moves to char16_t for
// that reason, and what it moves away from is wchar_t.
//
// It also carries a contract, and carrying it is what keeps text out of the
// heap: **the character after the text must be a zero**. A string handed to
// WinRT is handed over as a *fast-pass* string -- a header on the stack and a
// handle onto these very characters -- and that is what WindowsCreateStringReference
// asks in return. Without it every property set would copy its text into a
// fresh HSTRING, on every call, for nothing.
//
// Almost everything keeps it already: a literal, a std::basic_string of
// either unit (sta_wstring included), a wxl::wstring off a getter, and any
// view into an arena that terminates what it stores. What does not is a view
// into the middle of somebody else's buffer -- and the answer there is to
// terminate at the source (an arena writes one zero more; a parser puts a
// zero over the character that ends the run and puts it back), not to buy a
// copy on every call. A broken promise is loud rather than quiet: cppwinrt's
// create_hstring_on_stack aborts on the spot, in every build.
//
// Which leaves a seam, because the declarative syntax is written in L"..."
// throughout and every application built on wxl is written that way too. So
// a string parameter is neither view but a type that takes both:
//
//     block.text(u"already char16_t");   // where the code is going
//     window.title(L"still wchar_t");    // and where it is
//
// Nothing is copied and nothing is converted. On Windows the two units are
// the same sixteen bits under different type names, so crossing from one to
// the other is a renaming -- exactly the cast wxl already makes coming the
// other way, in impl/conversions.h, where an hstring is read as char16_t.
//
// Both pointer constructors are here rather than left to the view ones, and
// that is not redundancy: a bare L"..." is an array, and reaching a view
// from it and then this type would be two user-defined conversions, which
// the language does not do implicitly. With them it is one.

#include "core.h"

namespace wxl {

class string_param
{
public:
    // No text at all, which is what `text({})` says and what clears a
    // property. An empty view is a value like any other here, not a missing
    // one -- there is nothing to distinguish and nothing to ask.
    constexpr string_param() noexcept = default;

    // Where the code is going.
    constexpr string_param(std::u16string_view text) noexcept : text_{text} {}
    constexpr string_param(char16_t const* text) noexcept : text_{text} {}

    // And where it is. The cast is the whole of the conversion: same size,
    // same values, different type name -- and it is what keeps these two out
    // of constant evaluation, which a reinterpret_cast never enters.
    string_param(std::wstring_view text) noexcept
        : text_{reinterpret_cast<char16_t const*>(text.data()), text.size()} {}

    string_param(wchar_t const* text) noexcept : string_param{std::wstring_view{text}} {}

    // And strings of either unit, whatever allocator they carry -- which is
    // how wxl::wstring off a getter, an sta_wstring off a parser and a plain
    // std::wstring all arrive. They are named here for the same reason the
    // pointers are: a string reaches a view through a conversion of its own,
    // and that plus this one would again be two.
    template <typename Traits, typename Alloc>
    string_param(std::basic_string<char16_t, Traits, Alloc> const& text) noexcept
        : text_{text.data(), text.size()} {}

    template <typename Traits, typename Alloc>
    string_param(std::basic_string<wchar_t, Traits, Alloc> const& text) noexcept
        : text_{reinterpret_cast<char16_t const*>(text.data()), text.size()} {}

    // Checked text, as it is. The unit is already char16_t and the text is
    // already known well-formed, so it goes to the control without a step
    // back through wchars() -- which is how a model's u16_text reaches a
    // bound TextBox, and a window's title reaches the window.
    constexpr string_param(core::u16_view text) noexcept : text_{text.plain()} {}

    string_param(core::u16_text const& text) noexcept : text_{text.data(), text.size()} {}

    constexpr std::u16string_view text() const noexcept { return text_; }

    // For the one place that still has to hand WinRT a wchar_t view, which
    // is the conversion in impl/conversions.h and nowhere else.
    std::wstring_view wide() const noexcept {
        return {reinterpret_cast<wchar_t const*>(text_.data()), text_.size()};
    }

    constexpr bool empty() const noexcept { return text_.empty(); }

private:
    std::u16string_view text_;
};

}  // namespace wxl
