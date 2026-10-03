// Страница CaptureElementPreview -- CaptureElementPreviewPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <algorithm>
#include <string>
#include "Announce.h"
#include "CustomLayout.h"
#include "Failure.h"
#include "FrameSource.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Media.Imaging.h"
#include "generated/Microsoft.UI.Xaml.Media.h"
#include "generated/Windows.Media.Capture.Frames.h"
#include "generated/Windows.Media.Capture.h"
#include "generated/Windows.Media.Core.h"
#include "generated/Windows.Media.MediaProperties.h"
#include "generated/Windows.Media.Playback.h"
#include "generated/Windows.Storage.Streams.h"
#include "generated/Windows.System.h"

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
