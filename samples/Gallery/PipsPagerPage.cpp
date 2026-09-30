// Страница PipsPager -- PipsPagerPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t integratedHeader[] = {
#include "Snippets/PipsPager/IntegratedFlipView.html.embed"
};
constexpr char8_t integratedCode[] = {
#include "Snippets/PipsPager/IntegratedFlipView.h.embed"
};

FrameworkElement integrated() {
#include "Snippets/PipsPager/IntegratedFlipView.h"

    return gallery::controlExample({
        .header = gallery::snippet(integratedHeader),
        .example = example,
        .code = gallery::snippet(integratedCode),
    });
}

constexpr char8_t optionsHeader[] = {
#include "Snippets/PipsPager/Options.html.embed"
};
constexpr char8_t optionsCode[] = {
#include "Snippets/PipsPager/Options.h.embed"
};

FrameworkElement options() {
#include "Snippets/PipsPager/Options.h"

    return gallery::controlExample({
        .header = gallery::snippet(optionsHeader),
        .example = pager,
        .options = {options},
        .code = gallery::snippet(optionsCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::pipsPagerPage() {
    return StackPanel {integrated(), options()};
}
