// Страница SystemBackdrops -- SystemBackdropsPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t backdropTypesHeader[] = {
#include "Snippets/SystemBackdrops/SystemBackdropsBackdropTypes.html.embed"
};
constexpr char8_t backdropTypesCode[] = {
#include "Snippets/SystemBackdrops/SystemBackdropsBackdropTypes.h.embed"
};

FrameworkElement backdropTypes() {
#include "Snippets/SystemBackdrops/SystemBackdropsBackdropTypes.h"

    return gallery::controlExample({
        .header = gallery::snippet(backdropTypesHeader),
        .example = example,
        .code = gallery::snippet(backdropTypesCode),
    });
}

constexpr char8_t micaBackdropHeader[] = {
#include "Snippets/SystemBackdrops/SystemBackdropsMicacontroller.html.embed"
};
constexpr char8_t micaBackdropCode[] = {
#include "Snippets/SystemBackdrops/SystemBackdropsMicacontroller.h.embed"
};

FrameworkElement micaBackdropExample() {
#include "Snippets/SystemBackdrops/SystemBackdropsMicacontroller.h"

    return gallery::controlExample({
        .header = gallery::snippet(micaBackdropHeader),
        .example = example,
        .code = gallery::snippet(micaBackdropCode),
    });
}

constexpr char8_t acrylicBackdropHeader[] = {
#include "Snippets/SystemBackdrops/SystemBackdropsDesktopacryliccontroller.html.embed"
};
constexpr char8_t acrylicBackdropCode[] = {
#include "Snippets/SystemBackdrops/SystemBackdropsDesktopacryliccontroller.h.embed"
};

FrameworkElement acrylicBackdropExample() {
#include "Snippets/SystemBackdrops/SystemBackdropsDesktopacryliccontroller.h"

    return gallery::controlExample({
        .header = gallery::snippet(acrylicBackdropHeader),
        .example = example,
        .code = gallery::snippet(acrylicBackdropCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::systemBackdropsPage() {
    return StackPanel {backdropTypes(), micaBackdropExample(), acrylicBackdropExample()};
}
