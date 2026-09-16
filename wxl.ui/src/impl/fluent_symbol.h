#pragma once

#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include "../generated/FluentSymbol.h"

// What stands behind the second spelling of SymbolIcon's Symbol property.
//
// SymbolIcon takes a code point in SymbolThemeFontFamily and nothing else --
// the enumeration in its signature is a vocabulary, not a validated set. WinRT
// gives one vocabulary, `Symbol`, of 197 names chosen for Windows 8; wxl adds
// the other, `FluentSymbol`, of every name the font's own documentation
// carries. Crossing between them is a cast and could have been written at the
// call site, which is exactly the reason it is not: the cast is what makes a
// reader wonder whether the value is legitimate, and it is.
//
// Private: the icon arrives as the projection type the wrapper already holds,
// so this header is one only wxl's own sources ever include.

namespace wxl::impl {

// Draws `glyph` in the icon, the way setting Symbol would.
void set_fluent_symbol(winrt::Microsoft::UI::Xaml::Controls::SymbolIcon const& icon,
                       FluentSymbol glyph);

}  // namespace wxl::impl
