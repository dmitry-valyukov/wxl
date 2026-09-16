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
    // a font wherever a property asks for one. Both spellings of it, which
    // is string_param's whole job -- including the one user-defined
    // conversion an implicit sequence allows, so a bare literal still works.
    FontFamily(string_param name) noexcept : name_(name.text()) {}

    // The pointer overloads are what make a bare literal a font: reaching
    // string_param from one and this type from that would be two
    // user-defined conversions, which an implicit sequence never has.
    FontFamily(wchar_t const* name) noexcept : FontFamily(string_param{name}) {}
    FontFamily(char16_t const* name) noexcept : FontFamily(string_param{name}) {}

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
