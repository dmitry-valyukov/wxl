#pragma once

// wxl::frameSource -- one frame source of a started camera, by its id.
//
// MediaCapture.FrameSources is a read-only map of strings to sources, and nothing else in wxl is a map; the one
// thing a program asks of it is the source whose id a MediaFrameSourceInfo gave, to hand to a player as
// MediaSource::createFromMediaFrameSource. This is that question, asked of the map.

#include "generated/Windows.Media.Capture.Frames.h"
#include "generated/Windows.Media.Capture.h"
#include "hstring_param.h"

namespace wxl {

MediaFrameSource frameSource(MediaCapture const& capture, hstring_param const& id);

}  // namespace wxl
