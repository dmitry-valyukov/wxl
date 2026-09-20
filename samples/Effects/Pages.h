#pragma once

// Страницы образца: по эффекту на файл.
//
// Страница — `FrameworkElement`, и это самый удобный из четырёх возможных
// ответов. `Page` фреймворка живёт только внутри `Frame` и его навигации:
// ни того, ни другого wxl не выпускает, а переключает образец сам, подменяя
// содержимое одного `Border` справа от списка эффектов. `UIElement` — слишком
// мало: у него нет ни полей, ни выравнивания, ни `Loaded`, то есть ничего из
// того, чем страница себя размещает. `Panel` — слишком узко: корнем страницы законно бывает и
// `ScrollViewer`, и `Border`, и вообще не контейнер. Остаётся
// `FrameworkElement` — наименьшее, что умеет раскладываться, и ровно то, что
// принимает `Border::child`.
//
// Новый эффект — свой .cpp с такой же функцией, строка в `Pages.h` и строка
// в каталоге `main.cpp`. Раскладку страницы он не пишет: описание и примеры
// отдаются в `showcase`.

#include "pch.h"

#include <span>

namespace effects {

// Что кладут в каталог: функция, строящая страницу заново на каждый показ.
using Page = wxl::FrameworkElement (*)();

// Пример на странице эффекта: подпись, живой показ и его исходник. Исходник —
// разметка для RsdnBlock, а тот принимает только `std::wstring_view`.
struct Sample {
    const char16_t* title;
    const wchar_t* code;
    wxl::FrameworkElement (*build)();
};

// Страница эффекта: описание разметкой HTML сверху, примеры слева, исходник
// показанного примера справа (Showcase.cpp).
wxl::FrameworkElement showcase(const wchar_t* description, std::span<const Sample> samples);

// Halo Effect — свечение вокруг глифов (HaloPage.cpp).
wxl::FrameworkElement haloPage();

// Magnify Effect — элемент растёт под указателем, тем больше, чем ближе тот к
// центру (MagnifyPage.cpp).
wxl::FrameworkElement magnifyPage();

}  // namespace effects
