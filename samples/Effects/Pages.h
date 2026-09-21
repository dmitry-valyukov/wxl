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
// Новый эффект — свой .cpp с такой же функцией, папка сниппетов в
// `Snippets/`, строка в `Pages.h` и строка в каталоге `main.cpp`. Раскладку страницы он не пишет: описание и примеры
// отдаются в `showcase`.

#include "pch.h"

#include <span>
#include <string_view>

namespace effects {

// Что кладут в каталог: функция, строящая страницу заново на каждый показ.
using Page = wxl::FrameworkElement (*)();

// Кусок исходника примера: байты файла из Snippets/<эффект>/, вшитые в exe.
// Тот же файл включён по #include в функцию, которая строит пример, так что
// справа показано ровно то, что работает слева. Байты — `X.embed`, который
// CMake делает из `X.h` (`X.h.embed`, embed.cmake): это ровно то, что вернул
// бы `#embed "X.h"`, а #embed MSVC 14.51 пока не знает. char8_t, потому что байты
// идут числами 0..255, а `char` старше 127 в фигурных скобках — сужение.
using Snippet = std::u8string_view;

template <std::size_t N>
constexpr Snippet snippet(const char8_t (&bytes)[N]) {
    return Snippet{bytes, N};
}

// Пример на странице эффекта: подпись, исходник и живой показ. Исходник — до
// трёх кусков, по файлу на кусок: подготовка (переменные, лямбды) и само
// выражение; справа они идут подряд через пустую строку.
struct Sample {
    const char16_t* title;
    Snippet code[3];
    wxl::FrameworkElement (*build)();
};

// Страница эффекта: описание разметкой HTML сверху (Snippets/<эффект>/page.html), примеры слева, исходник
// показанного примера справа (Showcase.cpp).
wxl::FrameworkElement showcase(Snippet description, std::span<const Sample> samples);

// Halo Effect — свечение вокруг глифов (HaloPage.cpp).
wxl::FrameworkElement haloPage();

// Magnify Effect — элемент растёт или сжимается под указателем (MagnifyPage.cpp).
wxl::FrameworkElement magnifyPage();

}  // namespace effects
