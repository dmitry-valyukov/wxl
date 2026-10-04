#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Capture.Frames.h>
#include <winrt/Windows.Media.Capture.h>

#include "FrameSource.h"
#include "Object.impl.h"
#include <wxl/Windows.Media.Capture.Frames.impl.h>
#include <wxl/Windows.Media.Capture.impl.h>
#include "impl/conversions.h"

namespace wxl {

MediaFrameSource frameSource(MediaCapture const& capture, hstring_param const& id) {
    auto const sources = Object::Impl::as<winrt::Windows::Media::Capture::MediaCapture>(capture).FrameSources();
    return Object::Impl::wrap<MediaFrameSource>(sources.Lookup(impl::to_winrt(id)));
}

}  // namespace wxl
