module;

#include <format>
#include <ostream>
#include <print>

module wxl.gen;

import std;

// FluentSymbol.h: the named glyphs of the icon font as an enumeration, so an
// application writes `FontIcon { glyph = FluentSymbol::QuietHours }` rather
// than a code point nobody can read.
//
// This is the one enumeration wxl invents rather than projects. WinRT has
// `Symbol`, but it names 197 glyphs out of the two thousand the font carries,
// and the choice of which 197 was made for Windows 8. The names here are the
// framework's own, the whole documented range of Segoe Fluent Icons, and they
// arrive as data, from profiles/fluent-symbols.json, because the font itself
// carries no names to read (its `post` table is version 3.0, which holds
// none). That file says where they came from and which of them the font
// itself corrected.

namespace gen {

void write_symbols(Output const& out, Emitted& emitted) {
    auto const& symbols = symbol_names();
    if (symbols.empty()) {
        return;
    }

    auto header = open_output(out.dir / "FluentSymbol.h");
    std::print(header, "{}#pragma once\n\n#include <stdint.h>\n\nnamespace wxl {{\n\n", banner);
    // Hexadecimal, unlike the projected enumerations: every value here is a
    // code point, and 0xE700 is how the documentation, the font tools and the
    // XAML that spells it `&#xE700;` all write one.
    std::print(header, "enum class FluentSymbol : int32_t\n{{\n");
    for (auto&& symbol : symbols) {
        std::print(header, "    {} = 0x{:04X},\n", symbol.name, symbol.code);
    }
    std::print(header, "}};\n\n}} // namespace wxl\n");

    emitted.add("FluentSymbol.h");
    std::print("wrote {}FluentSymbol.h ({} symbols)\n", out.dir.string() + "\\", symbols.size());
}

}  // namespace gen
