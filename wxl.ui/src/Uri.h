#pragma once

// wxl::Uri -- what a declarative UI ever does with a URI, which is name one.
//
// A hand-written type rather than a wrapper over Windows.Foundation.Uri: the
// runtime type is a full COM object with a parser behind it, and a property
// that takes one is written in the DSL as a string literal and nothing else.
// Holding the text and building the real Uri on the way into the property
// keeps `NavigateUri = L"https://..."` to exactly those characters.

#include "generated/collections.h"

namespace wxl {

class Uri {
public:
    Uri() = default;

    // Not explicit, on purpose: the whole point is that a string literal is
    // a URI wherever a property asks for one. The pointer overload is what
    // makes the literal itself work -- through the view alone it would take
    // two user-defined conversions, which an implicit sequence never has.
    Uri(std::wstring_view text) noexcept
        : text_(reinterpret_cast<char16_t const*>(text.data()), text.size()) {}

    Uri(wchar_t const* text) noexcept : Uri(std::wstring_view{text}) {}

    // The reinterpret_cast is between wchar_t and char16_t, the same 16-bit
    // code unit on Windows differing only in type.
    std::wstring_view text() const noexcept {
        return {reinterpret_cast<wchar_t const*>(text_.data()), text_.size()};
    }

    bool empty() const noexcept { return text_.empty(); }

private:
    wstring text_;
};

}  // namespace wxl
