#pragma once

// The generator's short name for winmd's reader. The reader itself comes in
// with wxl.gen.common's crawl.h, which includes it after <windows.h> in the
// order everything included later needs; the profiles come with it.
#include "crawl.h"
#include "profile.h"

namespace md = winmd::reader;
