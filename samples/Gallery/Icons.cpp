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
            char32_t const point = static_cast<char32_t>(std::stoul(std::string {icon.code.begin(), icon.code.end()}, nullptr, 16));
            if (point < 0x10000) {
                icon.glyph.push_back(static_cast<char16_t>(point));
            } else {
                char32_t const rest = point - 0x10000;
                icon.glyph.push_back(static_cast<char16_t>(0xD800 + (rest >> 10)));
                icon.glyph.push_back(static_cast<char16_t>(0xDC00 + (rest & 0x3FF)));
            }
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
