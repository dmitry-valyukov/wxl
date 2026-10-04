#pragma once

// Значки шрифта Segoe Fluent Icons — IconsData.json оригинала, прочитанный один раз (IconData и IconsDataSource).

#include <string>
#include <vector>

namespace gallery {

struct Icon {
    std::u16string name;
    std::u16string code;      // шестнадцатеричный, как в файле: "E700"
    std::u16string glyph;     // сам знак (Character оригинала)
    std::vector<std::u16string> tags;
    bool segoeFluentOnly = false;
    bool isSymbol = false;    // имя значка есть и среди значений Symbol: тогда годится SymbolIcon
};

std::vector<Icon> const& icons();

}  // namespace gallery
