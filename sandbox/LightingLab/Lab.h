#pragma once

// Стенд освещения: то, что у опытов общее.
//
// Опыт -- функция, которая строит сцену, настройки и модель. Поля модели
// привязаны к настройкам, а её объекты композитора и рисования показаны на
// сцене. Модель держит оболочка, пока опыт на экране: привязка держит поле по
// адресу и им не владеет.

#include "pch.h"

namespace lab {

struct Experiment {
    wxl::FrameworkElement stage;
    wxl::FrameworkElement settings;
    std::shared_ptr<void> model;
};

using Build = Experiment (*)();

Experiment materialPage();  // MaterialPage.cpp
Experiment revealPage();    // RevealPage.cpp
Experiment direct2dPage();  // Direct2DPage.cpp
Experiment shaderPage();    // ShaderPage.cpp
Experiment shadowPage();    // ShadowPage.cpp
Experiment scenesPage();    // ScenesPage.cpp
Experiment samplePage();    // SamplePage.cpp
Experiment systemPage();    // SystemPage.cpp
Experiment keyPage();       // KeyPage.cpp

// Сцена опыта в DIP: одна у всех, чтобы опыты сравнивались глазом.
inline constexpr float stageWidth = 720.0f;
inline constexpr float stageHeight = 490.0f;

// Шесть клавиш сцены: три в ряд, два ряда.
inline constexpr int keyColumns = 3;
inline constexpr int keyRows = 2;
inline constexpr float keySide = 200.0f;
inline constexpr float keyGap = 30.0f;

inline constexpr float keyLeft(int column) { return keyGap + static_cast<float>(column) * (keySide + keyGap); }
inline constexpr float keyTop(int row) { return keyGap + static_cast<float>(row) * (keySide + keyGap); }

// --- Строки панели настроек (Controls.cpp) ---

wxl::FrameworkElement sliderRow(char16_t const* title, wxl::core::observable<double>& field, double low,
                                double high, double step);
wxl::FrameworkElement choiceRow(char16_t const* title, wxl::core::observable<int>& field,
                                std::span<char16_t const* const> names);
wxl::FrameworkElement toggleRow(char16_t const* title, wxl::core::observable<bool>& field);
wxl::FrameworkElement checkRow(char16_t const* title, wxl::core::observable<bool>& field);
wxl::FrameworkElement colorRow(char16_t const* title, wxl::core::observable<wxl::Color>& field);
wxl::FrameworkElement noteRow(char16_t const* words);

// Что система ответила на последнюю попытку опыта: «принято» или код отказа.
wxl::FrameworkElement statusRow(wxl::core::observable<wxl::hstring>& field);

// Сворачиваемая группа строк.
wxl::FrameworkElement group(char16_t const* title, bool open, std::initializer_list<wxl::FrameworkElement> rows);

// Панель настроек опыта: группы одна под другой.
wxl::FrameworkElement settingsPanel(std::initializer_list<wxl::FrameworkElement> groups);

wxl::hstring accepted(char16_t const* what);
wxl::hstring refusal(char16_t const* what, std::int32_t code, char16_t const* why);
wxl::hstring refusal(char16_t const* what, char16_t const* why);

// Отказ по исключению, которое сейчас ловится: код системы и её объяснение
// (Failure.cpp). Зовётся только из catch.
wxl::hstring refusalOfCurrentException(char16_t const* what);

// Делает попытку и пишет в поле, чем она кончилась. Опыт для того и ставится,
// чтобы узнать, примет ли система вызов, поэтому отказ -- это ответ, а не сбой.
template <class Action>
void attempt(wxl::core::observable<wxl::hstring>& status, char16_t const* what, Action&& action) noexcept {
    try {
        action();
        status.set(accepted(what));
    } catch (...) {
        status.set(refusalOfCurrentException(what));
    }
}

// Одно действие на изменение любого из полей.
template <class Action, class... Fields>
void onAny(Action action, Fields&... fields) {
    (static_cast<void>(fields.on_change([action](auto const&) noexcept { action(); })), ...);
}

// --- Карты нормалей и маски (NormalMaps.cpp) ---

enum class Relief { cushion, bevel, dish, dome, sphere, flat };

// Форма рельефа в долях стороны клавиши: скругление угла, ширина плеча и
// крутизна (тангенс наибольшего наклона). yUp -- зелёный канал карты растёт
// вверх экрана, а не вниз.
struct ReliefShape {
    double corner = 0.12;
    double shoulder = 0.12;
    double depth = 1.0;
    bool yUp = false;
};

// Пишет в поверхность карту нормалей квадратной клавиши: по пикселю на
// направление, красный -- x, зелёный -- y, синий -- z, все из [-1, 1] в
// [0, 1]. Карта непрозрачна целиком: за краем формы нормаль смотрит на
// зрителя, а форму клавише даёт обрезка визуала.
void drawNormalMap(wxl::DrawingSurface const& surface, Relief relief, ReliefShape const& shape);

// Клавиша Button3DEffect формой: тело, поднятое над панелью, с кантом по краю
// и лицом внутри него. Размеры -- в DIP.
struct KeyRelief {
    double width = 0.0;
    double height = 0.0;
    double corner = 0.0;   // скругление угла
    double rim = 0.0;      // ширина канта
    double slope = 1.0;    // тангенс наклона канта наружу
    bool rounded = false;  // кант -- четверть круга, а не ровная фаска
    double emboss = 0.0;   // лицо: чаша (меньше нуля), плоское, купол
    double smooth = 0.0;   // ширина полосы вокруг стыка лица с кантом, где один наклон переходит в другой
    bool sunk = false;     // кант обратный: не грани тела над панелью, а стенки колодца
};

// Пишет в поверхность карту нормалей такой клавиши -- во всю поверхность,
// какой бы плотности та ни была. Зелёный канал растёт вверх экрана, как у карт
// образцов Microsoft.
void drawKeyMap(wxl::DrawingSurface const& surface, KeyRelief const& key);

// Пишет в поверхность маску лица той же клавиши: белое лицо без канта. Край
// маски идёт по стыку лица с кантом и размыт на ту же ширину smooth, что и
// наклон в карте, но не уже одного пикселя поверхности.
void drawFaceMask(wxl::DrawingSurface const& surface, KeyRelief const& key);

// Заливает поверхность одним цветом.
void drawSolid(wxl::DrawingSurface const& surface, wxl::Color color);

// Кладёт в поверхность картинку из Assets/NormalMaps рядом с исполняемым файлом
// (Images.cpp); поверхность получает размер картинки.
void loadImage(wxl::DrawingSurface& surface, wchar_t const* name);

// Та же картинка пикселями: BGRA с умноженной альфой, строки сверху вниз.
struct Pixels {
    std::vector<std::uint32_t> pixels;
    int width = 0;
    int height = 0;
};
Pixels readImage(wchar_t const* name);

// Пишет в поверхность белый скруглённый прямоугольник во весь её размер: маска
// формы клавиши. Радиус -- в пикселях поверхности.
void drawShape(wxl::DrawingSurface const& surface, float radius);

}  // namespace lab
