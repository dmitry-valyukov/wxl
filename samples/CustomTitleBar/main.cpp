// wxl's own window with a title bar of its own, written the way a control is.
//
// The title bar is built right inside the window's braces: the window places
// it above the content, draws the caption buttons beside it at the height of
// the bar, and hands Windows the rectangles to drag by and to click. The
// window is a handle, so the zoom buttons below capture it by value, and the
// zoom enlarges the whole island -- bar, caption buttons and content -- the way
// a browser zooms a page.

#include "CompositionWindow.h"
#include "Panels.h"
#include "generated/brushes.h"
#include "launch.h"
#include "ui.h"

using namespace wxl;
using namespace wxl::dsl;

wxl::Teardown wxl_launched() {
    CompositionWindow const window{
        title = L"wxl: свой заголовок окна",
        minSize = {640, 400},
        extendsContentIntoTitleBar = true,
        titleBar = {
            background = brushes.SolidBackgroundFillColor.Secondary,
            leftHeader = TextBlock{L"wxl", vAlign.center, Margin{12, 0, 0, 0}},
            content = TextBox{placeholderText = L"Поиск", width = 320.0, vAlign.center},
            rightHeader = Button{L"Войти", vAlign.center, Margin{0, 0, 8, 0}},
        },
    };

    TextBlock const zoomText{L"100 %", vAlign.center, Margin{12, 0, 0, 0}};
    auto const zoomTo = [window, zoomText](double value) {
        window.zoom(value);
        zoomText.text(std::format(L"{:.0f} %", window.zoom() * 100));
    };

    window.content(StackPanel{
        Margin{24},
        spacing = 12.0,
        TextBlock{L"Кнопки окна рисует само окно, высотой полосы заголовка, и растут они вместе с ней."},
        StackPanel{
            orientation.horizontal,
            spacing = 8.0,
            Button{L"Крупнее", onClick = [window, zoomTo] { zoomTo(window.zoom() * 1.25); }},
            Button{L"Мельче", onClick = [window, zoomTo] { zoomTo(window.zoom() / 1.25); }},
            Button{L"100 %", onClick = [zoomTo] { zoomTo(1.0); }},
            zoomText,
        },
    });

    window.background(ARGB{0xFF, 0xF3, 0xF3, 0xF3});
    window.centreWithClientSize({960, 600});
    window.activate();

    return [window](TeardownReason) {};
}
