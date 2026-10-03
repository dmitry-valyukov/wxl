#include "RepeaterData.h"

namespace gallery {

using wxl::rgb;

std::vector<NamedColor> const& namedColors() {
    static std::vector<NamedColor> const colors = {
        {u"Blue", rgb(0, 0, 255)},
        {u"BlueViolet", rgb(138, 43, 226)},
        {u"Crimson", rgb(220, 20, 60)},
        {u"DarkCyan", rgb(0, 139, 139)},
        {u"DarkGoldenrod", rgb(184, 134, 11)},
        {u"DarkMagenta", rgb(139, 0, 139)},
        {u"DarkOliveGreen", rgb(85, 107, 47)},
        {u"DarkRed", rgb(139, 0, 0)},
        {u"DarkSlateBlue", rgb(72, 61, 139)},
        {u"DeepPink", rgb(255, 20, 147)},
        {u"IndianRed", rgb(205, 92, 92)},
        {u"MediumSlateBlue", rgb(123, 104, 238)},
        {u"Maroon", rgb(128, 0, 0)},
        {u"MidnightBlue", rgb(25, 25, 112)},
        {u"Peru", rgb(205, 133, 63)},
        {u"SaddleBrown", rgb(139, 69, 19)},
        {u"SteelBlue", rgb(70, 130, 180)},
        {u"OrangeRed", rgb(255, 69, 0)},
        {u"Firebrick", rgb(178, 34, 34)},
        {u"DarkKhaki", rgb(189, 183, 107)},
    };
    return colors;
}

std::vector<std::u16string> const& fruits() {
    static std::vector<std::u16string> const list = {u"Apricots", u"Bananas", u"Grapes", u"Strawberries", u"Watermelon", u"Plums", u"Blueberries"};
    return list;
}

std::vector<std::u16string> const& vegetables() {
    static std::vector<std::u16string> const list = {u"Broccoli", u"Spinach", u"Sweet potato", u"Cauliflower", u"Onion", u"Brussels sprouts", u"Carrots"};
    return list;
}

std::vector<std::u16string> const& grains() {
    static std::vector<std::u16string> const list = {u"Rice", u"Quinoa", u"Pasta", u"Bread", u"Farro", u"Oats", u"Barley"};
    return list;
}

std::vector<std::u16string> const& proteins() {
    static std::vector<std::u16string> const list = {u"Steak", u"Chicken", u"Tofu", u"Salmon", u"Pork", u"Chickpeas", u"Eggs"};
    return list;
}

namespace {

std::vector<std::u16string> const& extras() {
    static std::vector<std::u16string> const list = {u"Garlic", u"Lemon", u"Butter", u"Lime", u"Feta Cheese", u"Parmesan Cheese", u"Breadcrumbs"};
    return list;
}

// Простой генератор: одна и та же последовательность при каждом запуске.
struct Sequence {
    uint32_t state = 12345;

    int next(int below) {
        state = state * 1664525u + 1013904223u;
        return static_cast<int>((state >> 8) % static_cast<uint32_t>(below));
    }
};

bool contains(std::vector<std::u16string> const& list, std::u16string const& item) {
    for (auto const& each : list) {
        if (each == item) {
            return true;
        }
    }
    return false;
}

}  // namespace

std::vector<Recipe> makeRecipes(int count) {
    Sequence random;
    std::vector<Recipe> recipes;
    for (int k = 0; k < count; ++k) {
        Recipe recipe;
        recipe.num = k;
        recipe.name = u"Recipe " + std::u16string{};
        for (char16_t digit : std::to_string(k)) {
            recipe.name += digit;
        }
        recipe.colorIndex = random.next(static_cast<int>(namedColors().size()) - 1);

        // Одно блюдо каждого вида, как у оригинала, и несколько добавок, чтобы высоты были разными.
        recipe.ingredientList = {fruits()[random.next(6)], vegetables()[random.next(6)], grains()[random.next(6)], proteins()[random.next(6)]};
        for (auto const& each : recipe.ingredientList) {
            recipe.ingredients += u"\n" + each;
        }
        int const more = random.next(4);
        for (int i = 0; i < more; ++i) {
            auto const& extra = extras()[random.next(6)];
            if (!contains(recipe.ingredientList, extra)) {
                recipe.ingredients += u"\n" + extra;
                recipe.ingredientList.push_back(extra);
            }
        }
        recipes.push_back(std::move(recipe));
    }
    return recipes;
}

std::u16string numberText(int value) {
    std::u16string text;
    for (char digit : std::to_string(value)) {
        text += static_cast<char16_t>(digit);
    }
    return text;
}

bool containsIgnoringCase(std::u16string_view text, std::u16string_view part) {
    auto lower = [](char16_t c) { return c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c + (u'a' - u'A')) : c; };
    if (part.size() > text.size()) {
        return false;
    }
    for (std::size_t start = 0; start + part.size() <= text.size(); ++start) {
        std::size_t i = 0;
        while (i < part.size() && lower(text[start + i]) == lower(part[i])) {
            ++i;
        }
        if (i == part.size()) {
            return true;
        }
    }
    return false;
}

}  // namespace gallery
