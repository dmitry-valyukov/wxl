// Страница Sound -- SoundPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <string>
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Media.h>
#include <wxl/Microsoft.UI.Xaml.Documents.h>


using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t togglingHeader[] = {
#include "Snippets/Sound/TogglingSound.html.embed"
};
constexpr char8_t togglingCode[] = {
#include "Snippets/Sound/TogglingSound.h.embed"
};

FrameworkElement toggling() {
#include "Snippets/Sound/TogglingSound.h"

    return gallery::controlExample({
        .header = gallery::snippet(togglingHeader),
        .example = example,
        .code = gallery::snippet(togglingCode),
    });
}

constexpr char8_t spatialHeader[] = {
#include "Snippets/Sound/SoundTogglingSpatialAudio.html.embed"
};
constexpr char8_t spatialCode[] = {
#include "Snippets/Sound/SoundTogglingSpatialAudio.h.embed"
};

FrameworkElement spatial() {
#include "Snippets/Sound/SoundTogglingSpatialAudio.h"

    return gallery::controlExample({
        .header = gallery::snippet(spatialHeader),
        .example = example,
        .code = gallery::snippet(spatialCode),
    });
}

constexpr char8_t systemSoundHeader[] = {
#include "Snippets/Sound/PlaySpecificSystemSound.html.embed"
};
constexpr char8_t systemSoundCode[] = {
#include "Snippets/Sound/PlaySpecificSystemSound.h.embed"
};

FrameworkElement systemSound() {
#include "Snippets/Sound/PlaySpecificSystemSound.h"

    return gallery::controlExample({
        .header = gallery::snippet(systemSoundHeader),
        .example = example,
        .code = gallery::snippet(systemSoundCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::soundPage() {
    return StackPanel {toggling(), spatial(), systemSound()};
}
