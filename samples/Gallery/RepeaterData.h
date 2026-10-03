#pragma once

// Данные страницы ItemsRepeater: полоски первого примера, названия цветов, продукты и рецепты. У
// оригинала это классы Bar, Recipe и списки в ItemsRepeaterPage.xaml.cs; случайные числа оригинала
// здесь вычислены из позиции, чтобы страница была одна и та же при каждом запуске.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "Color.h"

namespace gallery {

struct Bar {
    Bar(double length, int max) : length(length), maxLength(max), height(length / 4), maxHeight(max / 4), diameter(length / 6), maxDiameter(max / 6) {}

    double length;
    int maxLength;
    double height;
    double maxHeight;
    double diameter;
    double maxDiameter;
};

struct Recipe {
    int num = 0;
    std::u16string name;
    std::u16string ingredients;
    std::vector<std::u16string> ingredientList;
    int colorIndex = 0;
};

struct NamedColor {
    char16_t const* name;
    wxl::Color color;
};

std::vector<NamedColor> const& namedColors();

std::vector<std::u16string> const& fruits();
std::vector<std::u16string> const& vegetables();
std::vector<std::u16string> const& grains();
std::vector<std::u16string> const& proteins();

std::vector<Recipe> makeRecipes(int count);

std::u16string numberText(int value);

/// Whether `text` holds `part`, upper and lower case of Latin letters taken as the same.
bool containsIgnoringCase(std::u16string_view text, std::u16string_view part);

}  // namespace gallery
