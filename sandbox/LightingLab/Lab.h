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

// Пишет в поверхность белое скруглённое кольцо во весь её размер: маска рамки
// для кисти девяти частей. Радиус и толщина -- в пикселях поверхности.
void drawRing(wxl::DrawingSurface const& surface, float radius, float thickness);

// Пишет в поверхность белый скруглённый прямоугольник во весь её размер: маска
// формы клавиши. Радиус -- в пикселях поверхности.
void drawShape(wxl::DrawingSurface const& surface, float radius);

}  // namespace lab
