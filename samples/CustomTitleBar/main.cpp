// wxl's own window with a title bar of its own, written the way a control is.
//
// The title bar is built right inside the window's braces: the window places
// it above the content, draws the caption buttons beside it at the height of
// the bar, and hands Windows the rectangles to drag by and to click. The zoom
// below enlarges the whole island -- bar, caption buttons and content -- the
// way a browser zooms a page.

#include "Bind.h"
#include "CompositionWindow.h"
#include "Panels.h"
#include "generated/brushes.h"
#include "launch.h"
#include "ui.h"

using namespace wxl;
using namespace wxl::core;
using namespace wxl::dsl;

namespace {

u16_text percent(double factor) {
    u16_text text = to_u16(factor * 100, std::chars_format::fixed, 0);
    text += u" %";
    return text;
}

// The zoom as two fields: the number the buttons set, and the caption that
// follows it. Bind hands a property the field's own type and converts nothing,
// so the turn from number to text is made here, by follow, and the TextBlock
// is bound to the text.
struct Zoom {
    observable<double> factor{1.0};
    observable<u16_text> caption;

    Zoom() { caption.follow(factor, percent); }
};

}  // namespace

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

    // The window is a handle, so the one watcher that zooms it captures it by
    // value; the buttons below hold the model and never see the window.
    auto const zoom = std::make_shared<Zoom>();
    zoom->factor.on_change([window](double const& value) noexcept { window.zoom(value); });

    window.content(StackPanel{
        Margin{24},
        spacing = 12.0,
        TextBlock{L"Кнопки окна рисует само окно, высотой полосы заголовка, и растут они вместе с ней."},
        StackPanel{
            orientation.horizontal,
            spacing = 8.0,
            Button{L"Крупнее", onClick = [zoom] { zoom->factor.set(zoom->factor.get() * 1.25); }},
            Button{L"Мельче", onClick = [zoom] { zoom->factor.set(zoom->factor.get() / 1.25); }},
            Button{L"100 %", onClick = [zoom] { zoom->factor.set(1.0); }},
            TextBlock{vAlign.center, Margin{12, 0, 0, 0}, text = BindOutput{zoom->caption}},
        },
    });

    window.background(rgb(243, 243, 243));
    window.centreWithClientSize({960, 600});
    window.activate();

    return [zoom](TeardownReason) {};
}
