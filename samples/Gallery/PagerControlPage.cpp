// Страница PagerControl -- PagerControlPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t basicHeader[] = {
#include "Snippets/PagerControl/Basic.html.embed"
};
constexpr char8_t basicCode[] = {
#include "Snippets/PagerControl/Basic.h.embed"
};

FrameworkElement basic() {
    return gallery::controlExample({
        .header = gallery::snippet(basicHeader),
        .example =
#include "Snippets/PagerControl/Basic.h"
        ,
        .code = gallery::snippet(basicCode),
    });
}

constexpr char8_t displayModesHeader[] = {
#include "Snippets/PagerControl/DisplayModes.html.embed"
};
constexpr char8_t displayModesCode[] = {
#include "Snippets/PagerControl/DisplayModes.h.embed"
};

FrameworkElement displayModes() {
    return gallery::controlExample({
        .header = gallery::snippet(displayModesHeader),
        .example =
#include "Snippets/PagerControl/DisplayModes.h"
        ,
        .code = gallery::snippet(displayModesCode),
    });
}

constexpr char8_t unboundedHeader[] = {
#include "Snippets/PagerControl/Unbounded.html.embed"
};
constexpr char8_t unboundedCode[] = {
#include "Snippets/PagerControl/Unbounded.h.embed"
};

FrameworkElement unbounded() {
    return gallery::controlExample({
        .header = gallery::snippet(unboundedHeader),
        .example =
#include "Snippets/PagerControl/Unbounded.h"
        ,
        .code = gallery::snippet(unboundedCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::pagerControlPage() {
    return StackPanel {basic(), displayModes(), unbounded()};
}
