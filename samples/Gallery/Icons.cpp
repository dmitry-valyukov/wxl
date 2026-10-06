#include "Icons.h"

#include "ApplicationFolder.h"

#include <string>

import std;
import wxl.core;
import wxl.json;

namespace gallery {

namespace {

// Имена значений Symbol (Enum.TryParse<Symbol> оригинала): имя значка, которого здесь нет, SymbolIcon не задаёт.
constexpr std::u16string_view symbolNames[] = {
#include "SymbolNames.inc"
};

std::u16string utf16(wxl::json::value const& field) {
    auto const text = field.as_string().to_utf16();
    return std::u16string {text.data(), text.size()};
}

std::vector<Icon> load() {
    std::vector<Icon> result;
    try {
        wxl::json::document document;
        auto const& root = document.load_file(wxl::applicationFolder() / L"Assets" / L"Data" / L"IconsData.json");
        for (auto const& node : root.elements()) {
            Icon icon;
            icon.name = utf16(node["Name"]);
            icon.code = utf16(node["Code"]);
            // Код знака читается из байтов файла как есть: он ASCII и
            // шестнадцатеричный, переводить его в UTF-16 и обратно незачем, а
            // поэлементная копия в std::string резала бы char16_t до char.
            // Строка, которая не код скалярного значения, — значок без знака:
            // показать его нечем, и он пропускается, а не роняет весь список.
            std::string_view const hex = node["Code"].as_string().chars();
            std::uint32_t point = 0;
            auto const [stop, error] = std::from_chars(hex.data(), hex.data() + hex.size(), point, 16);
            if (error != std::errc {} || stop != hex.data() + hex.size() || !wxl::core::unicode::is_scalar_value(point)) {
                continue;
            }
            wxl::core::unicode::append_utf16(icon.glyph, point);
            for (auto const& tag : node["Tags"].elements()) {
                icon.tags.push_back(utf16(tag));
            }
            icon.segoeFluentOnly = node["IsSegoeFluentOnly"].as_bool();
            icon.isSymbol = std::ranges::find(symbolNames, std::u16string_view {icon.name}) != std::end(symbolNames);
            result.push_back(std::move(icon));
        }
    } catch (std::exception const&) {
        result.clear();
    }
    return result;
}

}  // namespace

std::vector<Icon> const& icons() {
    static std::vector<Icon> const loaded = load();
    return loaded;
}

}  // namespace gallery
