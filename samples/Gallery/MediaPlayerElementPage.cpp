// Страница MediaPlayerElement -- MediaPlayerElementPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <wxl/Microsoft.UI.Content.h>
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.Windows.Storage.Pickers.h>
#include <wxl/Windows.Media.Core.h>
#include <wxl/Windows.Media.Playback.h>
#include <wxl/Windows.Storage.h>
#include "PickerFileTypes.h"

import wxl.async;


using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t transportControlsHeader[] = {
#include "Snippets/MediaPlayerElement/MediaplayerelementTransportControls.html.embed"
};
constexpr char8_t transportControlsCode[] = {
#include "Snippets/MediaPlayerElement/MediaplayerelementTransportControls.h.embed"
};

FrameworkElement transportControls() {
#include "Snippets/MediaPlayerElement/MediaplayerelementTransportControls.h"

    return gallery::controlExample({
        .header = gallery::snippet(transportControlsHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(transportControlsCode),
    });
}

constexpr char8_t autoplayHeader[] = {
#include "Snippets/MediaPlayerElement/MediaplayerelementAutoplaysVideo.html.embed"
};
constexpr char8_t autoplayCode[] = {
#include "Snippets/MediaPlayerElement/MediaplayerelementAutoplaysVideo.h.embed"
};

FrameworkElement autoplay() {
#include "Snippets/MediaPlayerElement/MediaplayerelementAutoplaysVideo.h"

    return gallery::controlExample({
        .header = gallery::snippet(autoplayHeader),
        .example = example,
        .code = gallery::snippet(autoplayCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::mediaPlayerElementPage() {
    return StackPanel {transportControls(), autoplay()};
}
