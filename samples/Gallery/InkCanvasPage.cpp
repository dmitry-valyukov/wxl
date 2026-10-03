// Страница InkCanvas -- InkCanvasPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <algorithm>
#include <initializer_list>
#include <string>
#include "RepeaterData.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Media.h"
#include "generated/Windows.Storage.Streams.h"
#include "generated/Windows.UI.Core.Enums.h"
#include "generated/Windows.UI.Input.Inking.h"

import wxl.async;


using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t drawingHeader[] = {
#include "Snippets/InkCanvas/InkCanvasDrawing.html.embed"
};
constexpr char8_t drawingCode[] = {
#include "Snippets/InkCanvas/InkCanvasDrawing.h.embed"
};

FrameworkElement drawing() {
#include "Snippets/InkCanvas/InkCanvasDrawing.h"

    return gallery::controlExample({
        .header = gallery::snippet(drawingHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(drawingCode),
    });
}

constexpr char8_t toolbarHeader[] = {
#include "Snippets/InkCanvas/InkCanvasToolbar.html.embed"
};
constexpr char8_t toolbarCode[] = {
#include "Snippets/InkCanvas/InkCanvasToolbar.h.embed"
};

FrameworkElement toolbar() {
#include "Snippets/InkCanvas/InkCanvasToolbar.h"

    return gallery::controlExample({
        .header = gallery::snippet(toolbarHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(toolbarCode),
    });
}

constexpr char8_t strokesHeader[] = {
#include "Snippets/InkCanvas/InkCanvasStrokes.html.embed"
};
constexpr char8_t strokesCode[] = {
#include "Snippets/InkCanvas/InkCanvasStrokes.h.embed"
};

FrameworkElement strokes() {
#include "Snippets/InkCanvas/InkCanvasStrokes.h"

    return gallery::controlExample({
        .header = gallery::snippet(strokesHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(strokesCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::inkCanvasPage() {
    return StackPanel {drawing(), toolbar(), strokes()};
}
