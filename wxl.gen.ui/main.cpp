// Визуальный редактор профилей генератора проекции.

#include "generated/brushes.h"
#include "launch.h"
#include "ui.h"

using namespace wxl;
using namespace wxl::dsl;

wxl::Teardown wxl_launched() {
    auto window = Window {
        title = u"wxl.gen.ui",
        Grid {
            background = brushes.SolidBackgroundFillColor.Base,
        },
    };

    window.appWindow().resize({1200, 800});
    window.activate();

    return {};
}
