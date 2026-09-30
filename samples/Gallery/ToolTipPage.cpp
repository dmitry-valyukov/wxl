// Страница ToolTip -- ToolTipPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t simpleHeader[] = {
#include "Snippets/ToolTip/ToolTipButton.html.embed"
};
constexpr char8_t simpleCode[] = {
#include "Snippets/ToolTip/ToolTipButton.h.embed"
};

FrameworkElement simple() {
    return gallery::controlExample({
        .header = gallery::snippet(simpleHeader),
        .example =
#include "Snippets/ToolTip/ToolTipButton.h"
        ,
        .code = gallery::snippet(simpleCode),
    });
}

constexpr char8_t placedHeader[] = {
#include "Snippets/ToolTip/ToolTipPlacement.html.embed"
};
constexpr char8_t placedCode[] = {
#include "Snippets/ToolTip/ToolTipPlacement.h.embed"
};

FrameworkElement placed() {
    return gallery::controlExample({
        .header = gallery::snippet(placedHeader),
        .example =
#include "Snippets/ToolTip/ToolTipPlacement.h"
        ,
        .code = gallery::snippet(placedCode),
    });
}

constexpr char8_t imageHeader[] = {
#include "Snippets/ToolTip/ToolTipImage.html.embed"
};
constexpr char8_t imageCode[] = {
#include "Snippets/ToolTip/ToolTipImage.h.embed"
};

FrameworkElement image() {
    return gallery::controlExample({
        .header = gallery::snippet(imageHeader),
        .example =
#include "Snippets/ToolTip/ToolTipImage.h"
        ,
        .code = gallery::snippet(imageCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::toolTipPage() {
    return StackPanel {simple(), placed(), image()};
}
