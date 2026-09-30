#pragma once

// Страницы Gallery: по контролу на файл, как в оригинале (Samples/<Контрол>/).
//
// Страница оригинала — `Page` в `Frame`; здесь она `FrameworkElement`, который
// окно кладёт в `NavigationView::content`, — наименьшее, что умеет
// раскладываться и что принимает содержимое. Раскладку одного примера
// (`ControlExample` оригинала) пишет `controlExample` из Presenter.cpp.
//
// Код каждого примера — файл `Snippets/<Контрол>/X.h`: он включается #include
// в функцию, строящую пример, и он же, вшитый байтами, показывается под
// примером. `X.h.embed` делает CMake (embed.cmake): это ровно то, что вернул
// бы `#embed "X.h"`, а MSVC 14.51 его пока не знает. Показанное и работающее —
// один текст. Введение к примеру — `X.html` рядом.

#include "Catalog.h"
#include "pch.h"

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace gallery {

// Байты файла из Snippets/, вшитые в exe. char8_t, потому что байты идут
// числами 0..255, а `char` старше 127 в фигурных скобках — сужение.
using Snippet = std::u8string_view;

template <std::size_t N>
constexpr Snippet snippet(const char8_t (&bytes)[N]) {
    return Snippet{bytes, N};
}

// Что кладёт в раскладку один пример.
struct ExampleParts {
    Snippet header;                      // введение, HTML
    wxl::FrameworkElement example;       // живой показ
    std::vector<wxl::FrameworkElement> output = {};   // подпись под показом (Output оригинала)
    std::vector<wxl::FrameworkElement> options = {};  // элементы управления справа (Options оригинала)
    Snippet code;                        // исходник показанного
};

// Один ControlExample: введение, показ с параметрами и исходник под ним.
wxl::FrameworkElement controlExample(ExampleParts const& parts);

// Функция, строящая страницу примеров одного контрола.
using ControlPage = wxl::FrameworkElement (*)();

// Страница контрола по идентификатору из каталога или nullptr, если контрол
// ещё не перенесён (в оригинале это IncludedInBuild).
ControlPage pageFor(std::wstring_view uniqueId);

// Страница контрола: примеры друг под другом.
wxl::FrameworkElement buttonPage();
wxl::FrameworkElement dropDownButtonPage();
wxl::FrameworkElement hyperlinkButtonPage();
wxl::FrameworkElement repeatButtonPage();
wxl::FrameworkElement toggleButtonPage();
wxl::FrameworkElement splitButtonPage();
wxl::FrameworkElement toggleSplitButtonPage();
wxl::FrameworkElement checkBoxPage();
wxl::FrameworkElement colorPickerPage();
wxl::FrameworkElement comboBoxPage();
wxl::FrameworkElement radioButtonPage();
wxl::FrameworkElement ratingControlPage();
wxl::FrameworkElement sliderPage();
wxl::FrameworkElement toggleSwitchPage();

// Плитка контрола (ControlItemTemplate оригинала) и сетка плиток: GridView,
// клик по плитке — переход на страницу контрола.
wxl::FrameworkElement controlTile(ControlInfo const& item);
wxl::FrameworkElement tileGrid(std::span<ControlInfo const* const> items, wxl::Thickness padding);

// Текст как разметка: `&`, `<` и `>` заменены сущностями.
std::wstring htmlEscape(std::wstring_view text);

// Строка из WinRT (char16_t) как wchar_t, каким пользуется каталог.
inline std::wstring wide(std::u16string_view text) {
    return std::wstring(text.begin(), text.end());
}

// Служебные страницы оригинала (Pages/).
wxl::FrameworkElement homePage();
wxl::FrameworkElement allControlsPage();
wxl::FrameworkElement sectionPage(ControlGroup const& group);
wxl::FrameworkElement itemPage(ControlInfo const& item);
wxl::FrameworkElement searchResultsPage(std::wstring_view query);
wxl::FrameworkElement settingsPage();

}  // namespace gallery
