#pragma once

// wxl::Uri -- what a declarative UI ever does with a URI, which is name one.
//
// A hand-written type rather than a wrapper over Windows.Foundation.Uri: the
// runtime type is a full COM object with a parser behind it, and a property
// that takes one is written in the DSL as a string literal and nothing else.
// Holding the text and building the real Uri on the way into the property
// keeps `NavigateUri = L"https://..."` to exactly those characters.

#include "generated/collections.h"
#include "string_param.h"

namespace wxl {

class Uri {
public:
    Uri() = default;

    // Not explicit, on purpose: the whole point is that a string literal is
    // a URI wherever a property asks for one. What counts as a string is
    // string_param's business and nobody else's here. It is a template, not
    // a `Uri(string_param)`, because through that a literal would take two
    // user-defined conversions (literal to string_param, string_param to Uri),
    // which an implicit sequence never has; deduced, the literal arrives as
    // itself and string_param is the one conversion inside.
    template <typename Text>
        requires std::convertible_to<Text const&, string_param>
    Uri(Text const& text) noexcept : Uri{Own{}, string_param{text}} {}

    // The reinterpret_cast is between wchar_t and char16_t, the same 16-bit
    // code unit on Windows differing only in type.
    std::wstring_view text() const noexcept {
        return {reinterpret_cast<wchar_t const*>(text_.data()), text_.size()};
    }

    bool empty() const noexcept { return text_.empty(); }

private:
    struct Own {};

    Uri(Own, string_param text) noexcept : text_(text.text().data(), text.text().size()) {}

    wstring text_;
};

}  // namespace wxl
