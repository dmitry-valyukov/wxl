// Страница StandardUICommand -- StandardUICommandPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t exposingHeader[] = {
#include "Snippets/StandardUICommand/ExposingCommand.html.embed"
};
constexpr char8_t exposingCode[] = {
#include "Snippets/StandardUICommand/ExposingCommand.h.embed"
};

FrameworkElement exposing() {
#include "Snippets/StandardUICommand/ExposingCommand.h"

    return gallery::controlExample({
        .header = gallery::snippet(exposingHeader),
        .example = example,
        .code = gallery::snippet(exposingCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::standardUICommandPage() {
    return StackPanel {exposing()};
}
