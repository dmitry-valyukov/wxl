// Страница AppWindow -- AppWindowPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include "WindowModal.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t creatingHeader[] = {
#include "Snippets/AppWindow/CreatingCustomizingAppwindowWindow.html.embed"
};
constexpr char8_t creatingCode[] = {
#include "Snippets/AppWindow/CreatingCustomizingAppwindowWindow.h.embed"
};

FrameworkElement creating() {
#include "Snippets/AppWindow/CreatingCustomizingAppwindowWindow.h"

    return gallery::controlExample({
        .header = gallery::snippet(creatingHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(creatingCode),
    });
}

constexpr char8_t centeringHeader[] = {
#include "Snippets/AppWindow/CenteringAppwindowScreenAvailable.html.embed"
};
constexpr char8_t centeringCode[] = {
#include "Snippets/AppWindow/CenteringAppwindowScreenAvailable.h.embed"
};

FrameworkElement centering() {
#include "Snippets/AppWindow/CenteringAppwindowScreenAvailable.h"

    return gallery::controlExample({
        .header = gallery::snippet(centeringHeader),
        .example = example,
        .code = gallery::snippet(centeringCode),
    });
}

constexpr char8_t overlappedHeader[] = {
#include "Snippets/AppWindow/AppwindowOverlapedpresenter.html.embed"
};
constexpr char8_t overlappedCode[] = {
#include "Snippets/AppWindow/AppwindowOverlapedpresenter.h.embed"
};

FrameworkElement overlapped() {
#include "Snippets/AppWindow/AppwindowOverlapedpresenter.h"

    return gallery::controlExample({
        .header = gallery::snippet(overlappedHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(overlappedCode),
    });
}

constexpr char8_t minMaxHeader[] = {
#include "Snippets/AppWindow/AppWindowSettingMinimumMaximumWidth.html.embed"
};
constexpr char8_t minMaxCode[] = {
#include "Snippets/AppWindow/AppWindowSettingMinimumMaximumWidth.h.embed"
};

FrameworkElement minMax() {
#include "Snippets/AppWindow/AppWindowSettingMinimumMaximumWidth.h"

    return gallery::controlExample({
        .header = gallery::snippet(minMaxHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(minMaxCode),
    });
}

constexpr char8_t modalHeader[] = {
#include "Snippets/AppWindow/ModalWindowOverlappedpresenterAppwindow.html.embed"
};
constexpr char8_t modalCode[] = {
#include "Snippets/AppWindow/ModalWindowOverlappedpresenterAppwindow.h.embed"
};

FrameworkElement modal() {
#include "Snippets/AppWindow/ModalWindowOverlappedpresenterAppwindow.h"

    return gallery::controlExample({
        .header = gallery::snippet(modalHeader),
        .example = example,
        .code = gallery::snippet(modalCode),
    });
}

constexpr char8_t fullscreenHeader[] = {
#include "Snippets/AppWindow/AppwindowFullscreenpresenter.html.embed"
};
constexpr char8_t fullscreenCode[] = {
#include "Snippets/AppWindow/AppwindowFullscreenpresenter.h.embed"
};

FrameworkElement fullscreen() {
#include "Snippets/AppWindow/AppwindowFullscreenpresenter.h"

    return gallery::controlExample({
        .header = gallery::snippet(fullscreenHeader),
        .example = example,
        .code = gallery::snippet(fullscreenCode),
    });
}

constexpr char8_t compactOverlayHeader[] = {
#include "Snippets/AppWindow/AppwindowCompactoverlaypresenter.html.embed"
};
constexpr char8_t compactOverlayCode[] = {
#include "Snippets/AppWindow/AppwindowCompactoverlaypresenter.h.embed"
};

FrameworkElement compactOverlay() {
#include "Snippets/AppWindow/AppwindowCompactoverlaypresenter.h"

    return gallery::controlExample({
        .header = gallery::snippet(compactOverlayHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(compactOverlayCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::appWindowPage() {
    return StackPanel {
        TextBlock {Margin {0, 16, 0, -16}, styles.TextBlock.Subtitle, u"General usage of AppWindow"},
        creating(),
        centering(),
        TextBlock {Margin {0, 24, 0, -16}, styles.TextBlock.Subtitle, u"AppWindow Presenters"},
        overlapped(),
        minMax(),
        modal(),
        fullscreen(),
        compactOverlay(),
    };
}
