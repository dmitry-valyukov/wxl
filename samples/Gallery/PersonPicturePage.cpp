// Страница PersonPicture -- PersonPicturePage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Media.Imaging.h>


using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t looksHeader[] = {
#include "Snippets/PersonPicture/PersonPictureSelectDifferentLooksPerson.html.embed"
};
constexpr char8_t looksCode[] = {
#include "Snippets/PersonPicture/PersonPictureSelectDifferentLooksPerson.h.embed"
};

FrameworkElement looks() {
#include "Snippets/PersonPicture/PersonPictureSelectDifferentLooksPerson.h"

    return gallery::controlExample({
        .header = gallery::snippet(looksHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(looksCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::personPicturePage() {
    return StackPanel {looks()};
}
