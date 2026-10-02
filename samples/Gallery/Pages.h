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
#include <string>
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

// Рост под указателем для кнопок самого приложения (заголовок страницы,
// настройки, копирование кода), а не примеров: как у кнопок панели в примере
// Effects.
inline wxl::MagnifyEffect appPop() {
    return wxl::MagnifyEffect {1.2, wxl::dsl::maximum = 1.35, wxl::dsl::minimum = 0.95};
}

// Страница-образец окон примеров Windowing (SamplePage1..4 оригинала): плитки
// и абзац текста; номер — от 1 до 4.
wxl::FrameworkElement samplePage(int number);

// Выбор цвета: образец на кнопке и ColorPicker в её выпадающей части
// (ColorSelector оригинала). Цвет — поле модели.
wxl::FrameworkElement colorSelector(wxl::core::observable<wxl::Color>& color, char16_t const* name);

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
wxl::FrameworkElement textBlockPage();
wxl::FrameworkElement textBoxPage();
wxl::FrameworkElement passwordBoxPage();
wxl::FrameworkElement numberBoxPage();
wxl::FrameworkElement autoSuggestBoxPage();
wxl::FrameworkElement borderPage();
wxl::FrameworkElement canvasPage();
wxl::FrameworkElement expanderPage();
wxl::FrameworkElement gridPage();
wxl::FrameworkElement relativePanelPage();
wxl::FrameworkElement splitViewPage();
wxl::FrameworkElement stackPanelPage();
wxl::FrameworkElement variableSizedWrapGridPage();
wxl::FrameworkElement viewboxPage();
wxl::FrameworkElement listViewPage();
wxl::FrameworkElement gridViewPage();
wxl::FrameworkElement flipViewPage();
wxl::FrameworkElement treeViewPage();
wxl::FrameworkElement pullToRefreshPage();
wxl::FrameworkElement contentIslandPage();
wxl::FrameworkElement jumpListPage();
wxl::FrameworkElement storagePickersPage();
wxl::FrameworkElement clipboardPage();
wxl::FrameworkElement appNotificationPage();
wxl::FrameworkElement badgeNotificationManagerPage();
wxl::FrameworkElement compositionWindowPage();
wxl::FrameworkElement systemBackdropsPage();
wxl::FrameworkElement appWindowPage();
wxl::FrameworkElement appWindowTitleBarPage();
wxl::FrameworkElement titleBarPage();
wxl::FrameworkElement windowingPage();
wxl::FrameworkElement compactSizingPage();
wxl::FrameworkElement systemBackdropElementPage();
wxl::FrameworkElement acrylicPage();
wxl::FrameworkElement animatedIconPage();
wxl::FrameworkElement radialGradientBrushPage();
wxl::FrameworkElement linePage();
wxl::FrameworkElement shapePage();
wxl::FrameworkElement iconElementPage();
wxl::FrameworkElement themeShadowPage();
wxl::FrameworkElement scrollViewPage();
wxl::FrameworkElement annotatedScrollBarPage();
wxl::FrameworkElement pipsPagerPage();
wxl::FrameworkElement pagerControlPage();
wxl::FrameworkElement scrollViewerPage();
wxl::FrameworkElement calendarViewPage();
wxl::FrameworkElement timePickerPage();
wxl::FrameworkElement datePickerPage();
wxl::FrameworkElement calendarDatePickerPage();
wxl::FrameworkElement standardUICommandPage();
wxl::FrameworkElement xamlUICommandPage();
wxl::FrameworkElement swipeControlPage();
wxl::FrameworkElement commandBarFlyoutPage();
wxl::FrameworkElement menuFlyoutPage();
wxl::FrameworkElement menuBarPage();
wxl::FrameworkElement commandBarPage();
wxl::FrameworkElement layoutPanelPage();
wxl::FrameworkElement wrapPanelPage();
wxl::FrameworkElement appBarSeparatorPage();
wxl::FrameworkElement appBarToggleButtonPage();
wxl::FrameworkElement appBarButtonPage();
wxl::FrameworkElement toolTipPage();
wxl::FrameworkElement progressRingPage();
wxl::FrameworkElement progressBarPage();
wxl::FrameworkElement infoBarPage();
wxl::FrameworkElement infoBadgePage();
wxl::FrameworkElement richTextBlockPage();
wxl::FrameworkElement richEditBoxPage();

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

// Каталог для примеров с поиском: все слова запроса есть в тексте (без учёта
// регистра); названия перенесённых контролов, подходящие под запрос; контрол
// по названию; путь к картинке из каталога (`ms-appx:///Assets/…`).
bool containsWords(std::u16string_view text, std::u16string_view query);
std::vector<std::u16string> controlTitles(std::u16string_view query);
ControlInfo const* controlByTitle(std::u16string_view title);
std::wstring assetPath(std::wstring_view imagePath);

// XML с отступами по вложенности: элемент с одним текстом остаётся в строке.
std::wstring indentXml(std::wstring_view xml);

// Служебные страницы оригинала (Pages/).
wxl::FrameworkElement homePage();
wxl::FrameworkElement allControlsPage();
wxl::FrameworkElement sectionPage(ControlGroup const& group);
wxl::FrameworkElement itemPage(ControlInfo const& item);
wxl::FrameworkElement searchResultsPage(std::wstring_view query);
wxl::FrameworkElement settingsPage();

}  // namespace gallery
