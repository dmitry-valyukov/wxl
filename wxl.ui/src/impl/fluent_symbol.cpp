#include "fluent_symbol.h"

namespace wxl::impl {

void set_fluent_symbol(winrt::Microsoft::UI::Xaml::Controls::SymbolIcon const& icon,
                       FluentSymbol glyph) {
    // Both enumerations are int32_t code points into the same font, and the
    // ranges do not even meet -- Symbol sits in E1xx, FluentSymbol from E700
    // up -- so nothing is being reinterpreted here beyond the name.
    icon.Symbol(static_cast<winrt::Microsoft::UI::Xaml::Controls::Symbol>(glyph));
}

}  // namespace wxl::impl
