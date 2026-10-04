#pragma once

// Опоры страниц раздела Design: картинка, меняющаяся с темой, кнопки-подсказки
// на ней, строки таблиц и кнопка копирования имени ресурса (Typography,
// Geometry, Spacing, Color оригинала).

#include "pch.h"

#include <string>
#include <vector>

namespace gallery {

// Кнопка-значок, что кладёт имя ресурса в буфер обмена.
wxl::FrameworkElement copyNameButton(std::u16string name);

// Подпись столбца таблицы.
wxl::FrameworkElement designColumnTitle(char16_t const* text, int column = 0, double left = 0);

// Картинка Assets/Design/<stem>.dark.png или .light.png — по теме, в которой
// она стоит.
wxl::Image themedImage(std::u16string stem, double height);

// Кнопка-значок на картинке: открывает подпись рядом с собой.
struct Pin {
    double left;
    double top;
    std::u16string title;
    std::u16string subtitle = {};
    std::u16string tooltip = {};
};

// Картинка с кнопками-подсказками поверх, в прокрутке по горизонтали.
wxl::FrameworkElement pinnedImage(std::u16string stem, double width, double height, std::vector<Pin> pins);

// Строка таблицы: фон-карточка через одну.
wxl::Grid designRow(bool shaded, wxl::Thickness margin = {});

// Строка таблицы типографики.
template <class Style>
wxl::FrameworkElement typographyRow(char16_t const* example, Style style, char16_t const* resource, char16_t const* font,
                                    char16_t const* size, bool shaded) {
    using namespace wxl;
    using namespace wxl::dsl;
    auto cell = [](char16_t const* text, int at) {
        return TextBlock {column = at, vAlign.center, styles.TextBlock.Caption, text};
    };
    auto name = cell(resource, 3);
    name.isTextSelectionEnabled(true);
    name.fontFamily(u"Consolas");
    auto row = designRow(shaded);
    row.padding(Thickness {0, 12, 0, 12});
    row.columnDefinitions(u"272,136,112,194,auto");
    row.children().append(TextBlock {Margin {16, 0, 0, 0}, vAlign.center, style, example});
    row.children().append(cell(font, 1));
    row.children().append(cell(size, 2));
    row.children().append(name);
    row.children().append(Border {column = 4, Margin {4, 2, 8, 0}, copyNameButton(resource)});
    return row;
}

// Строка таблицы скруглений: образец, где применяется, имя ресурса.
wxl::FrameworkElement geometryRow(double radius, char16_t const* value, char16_t const* usage, char16_t const* resource,
                                  bool shaded);

}  // namespace gallery
