// Страница HeaderedContentControl Community Toolkit: контрол wxl (по образцу SettingsCard, decisions/0465).

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t basicHeader[] = {
#include "Snippets/HeaderedContentControl/HeaderedContentControlBasic.html.embed"
};
constexpr char8_t basicCode[] = {
#include "Snippets/HeaderedContentControl/HeaderedContentControlBasic.h.embed"
};

FrameworkElement basicExample() {
#include "Snippets/HeaderedContentControl/HeaderedContentControlBasic.h"

    return gallery::controlExample({.header = gallery::snippet(basicHeader), .example = example, .code = gallery::snippet(basicCode)});
}

}  // namespace

wxl::FrameworkElement gallery::headeredContentControlPage() {
    return StackPanel {basicExample()};
}
