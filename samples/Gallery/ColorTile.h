#pragma once

// Плитка цвета и пример цвета страницы Color — ColorTile и ColorPageExample оригинала. Кисть приходит
// типом из `brushes` (значит, следует за темой), `nullptr` — кисти нет, цвет `rgb` — кисть из цвета.

#include "pch.h"

#include <string_view>
#include <type_traits>
#include <vector>

namespace gallery {

enum class ColorBackdrop { None, Acrylic, Mica, MicaAlt };

struct ColorTileInfo {
    char16_t const* name;
    char16_t const* explanation;
    char16_t const* key;  // имя ресурса-кисти; его кладёт в буфер кнопка плитки
    bool separator;
    ColorBackdrop backdrop;
    bool comment;  // «См. SystemBackdrop и SystemBackdropElement»
    int row;
    int column;
};

// Что построено, пока кисти не надеты: надевает их шаблон, он один знает тип кисти.
struct ColorTileParts {
    wxl::Grid root;
    wxl::Grid body;
    std::vector<wxl::TextBlock> texts;
    wxl::FontIcon copyIcon;
};

struct ColorExampleParts {
    wxl::Grid root;
    wxl::TextBlock title;
    wxl::TextBlock description;
};

ColorTileParts makeColorTile(ColorTileInfo const& info);
ColorExampleParts makeColorExample(char16_t const* exampleTitle, char16_t const* description, wxl::FrameworkElement const& content);

template <class Background, class Foreground>
wxl::FrameworkElement colorTile(Background const& background, Foreground const& foreground, ColorTileInfo const& info) {
    auto parts = makeColorTile(info);
    if constexpr (!std::is_null_pointer_v<Background>) {
        wxl::Preset {wxl::dsl::background = background}(parts.body);
    }
    if constexpr (!std::is_null_pointer_v<Foreground>) {
        for (auto const& text : parts.texts) {
            wxl::Preset {wxl::dsl::foreground = foreground}(text);
        }
        wxl::Preset {wxl::dsl::foreground = foreground}(parts.copyIcon);
    }
    return parts.root;
}

template <class Background, class Foreground>
wxl::FrameworkElement colorExample(char16_t const* exampleTitle, char16_t const* description, Background const& background,
                                   Foreground const& foreground, wxl::FrameworkElement const& content) {
    auto parts = makeColorExample(exampleTitle, description, content);
    if constexpr (!std::is_null_pointer_v<Background>) {
        wxl::Preset {wxl::dsl::background = background}(parts.root);
    }
    if constexpr (!std::is_null_pointer_v<Foreground>) {
        wxl::Preset {wxl::dsl::foreground = foreground}(parts.title);
        wxl::Preset {wxl::dsl::foreground = foreground}(parts.description);
    }
    return parts.root;
}

// Сетка плиток в рамке (GalleryTileGridStyle).
wxl::FrameworkElement tileGrid(int columns, int rows, std::vector<wxl::FrameworkElement> tiles);

// Что показывает пример с таким заголовком (содержимое ColorPageExample).
wxl::FrameworkElement colorSample(char16_t const* sampleTitle);

// Шесть разделов страницы Color.
wxl::FrameworkElement textSection();
wxl::FrameworkElement fillSection();
wxl::FrameworkElement strokeSection();
wxl::FrameworkElement backgroundSection();
wxl::FrameworkElement signalSection();
wxl::FrameworkElement highContrastSection();

}  // namespace gallery
