#pragma once

#include "generated/collections.h"
#include "string_param.h"

// wxl::FontFamily -- a font is named, and naming it is all a declarative UI
// ever does with one.
//
// A hand-written type rather than a wrapper over Microsoft.UI.Xaml.Media
// .FontFamily: the runtime type is a COM object built from that name and
// offering nothing else worth reaching, so wxl keeps the text and builds the
// real one on the way into the property. That is what makes
// `fontFamily = L"Cascadia Mono"` exactly those characters.
//
// The name is what XAML writes too, including its packaged form for a font
// shipped with the application -- "ms-appx:///Assets/Fonts/Name.ttf#Family".

namespace wxl {

class FontFamily {
public:
    FontFamily() = default;

    // Not explicit, on purpose: the whole point is that a string literal is
    // a font wherever a property asks for one. What counts as a string is
    // string_param's business; it is a template, not `FontFamily(string_param)`,
    // because through that a literal or a std::wstring would take two
    // user-defined conversions, which an implicit sequence never has.
    template <typename Text>
        requires std::convertible_to<Text const&, string_param>
    FontFamily(Text const& name) noexcept : name_(string_param{name}.text()) {}

    // The reinterpret_cast is between wchar_t and char16_t, the same 16-bit
    // code unit on Windows differing only in type.
    std::wstring_view name() const noexcept {
        return {reinterpret_cast<wchar_t const*>(name_.data()), name_.size()};
    }

    bool empty() const noexcept { return name_.empty(); }

private:
    wstring name_;
};

}  // namespace wxl
