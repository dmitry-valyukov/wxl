// Страница CaptureElementPreview -- CaptureElementPreviewPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <algorithm>
#include <string>
#include "Announce.h"
#include "CustomLayout.h"
#include "Failure.h"
#include "FrameSource.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Media.Imaging.h>
#include <wxl/Microsoft.UI.Xaml.Media.h>
#include <wxl/Windows.Media.Capture.Frames.h>
#include <wxl/Windows.Media.Capture.h>
#include <wxl/Windows.Media.Core.h>
#include <wxl/Windows.Media.MediaProperties.h>
#include <wxl/Windows.Media.Playback.h>
#include <wxl/Windows.Storage.Streams.h>
#include <wxl/Windows.System.h>

import wxl.async;


using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t previewHeader[] = {
#include "Snippets/CaptureElementPreview/CaptureElementPreviewMediacapturePreviewDisplayedVia.html.embed"
};
constexpr char8_t previewCode[] = {
#include "Snippets/CaptureElementPreview/CaptureElementPreviewMediacapturePreviewDisplayedVia.h.embed"
};

FrameworkElement preview() {
#include "Snippets/CaptureElementPreview/CaptureElementPreviewMediacapturePreviewDisplayedVia.h"

    return gallery::controlExample({
        .header = gallery::snippet(previewHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(previewCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::captureElementPreviewPage() {
    return StackPanel {preview()};
}
