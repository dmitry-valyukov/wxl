// Страница CompositionWindow -- CompositionWindowPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include "ApplicationFolder.h"
#include "CompositionWindow.h"
#include <d2d1_1.h>

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t creatingHeader[] = {
#include "Snippets/CompositionWindow/CreatingWindow.html.embed"
};
constexpr char8_t creatingCode[] = {
#include "Snippets/CompositionWindow/CreatingWindow.h.embed"
};

FrameworkElement creating() {
#include "Snippets/CompositionWindow/CreatingWindow.h"

    return gallery::controlExample({
        .header = gallery::snippet(creatingHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(creatingCode),
    });
}

constexpr char8_t sceneBackgroundHeader[] = {
#include "Snippets/CompositionWindow/SceneBackground.html.embed"
};
constexpr char8_t sceneBackgroundCode[] = {
#include "Snippets/CompositionWindow/SceneBackground.h.embed"
};

FrameworkElement sceneBackground() {
#include "Snippets/CompositionWindow/SceneBackground.h"

    return gallery::controlExample({
        .header = gallery::snippet(sceneBackgroundHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(sceneBackgroundCode),
    });
}

constexpr char8_t visualsHeader[] = {
#include "Snippets/CompositionWindow/SceneVisuals.html.embed"
};
constexpr char8_t visualsCode[] = {
#include "Snippets/CompositionWindow/SceneVisuals.h.embed"
};

FrameworkElement visuals() {
#include "Snippets/CompositionWindow/SceneVisuals.h"

    return gallery::controlExample({
        .header = gallery::snippet(visualsHeader),
        .example = example,
        .code = gallery::snippet(visualsCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::compositionWindowPage() {
    return StackPanel {creating(), sceneBackground(), visuals()};
}
