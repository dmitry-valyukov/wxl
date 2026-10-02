// Страница JumpList -- JumpListPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include "Packaged.h"
#include "generated/Windows.UI.StartScreen.h"

import wxl.async;

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t tasksHeader[] = {
#include "Snippets/JumpList/AddingTasksJumpList.html.embed"
};
constexpr char8_t tasksCode[] = {
#include "Snippets/JumpList/AddingTasksJumpList.h.embed"
};

FrameworkElement tasks() {
#include "Snippets/JumpList/AddingTasksJumpList.h"

    return gallery::controlExample({
        .header = gallery::snippet(tasksHeader),
        .example = example,
        .code = gallery::snippet(tasksCode),
    });
}

constexpr char8_t groupHeader[] = {
#include "Snippets/JumpList/JumpListAddingItemsCustomGroup.html.embed"
};
constexpr char8_t groupCode[] = {
#include "Snippets/JumpList/JumpListAddingItemsCustomGroup.h.embed"
};

FrameworkElement group() {
#include "Snippets/JumpList/JumpListAddingItemsCustomGroup.h"

    return gallery::controlExample({
        .header = gallery::snippet(groupHeader),
        .example = example,
        .code = gallery::snippet(groupCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::jumpListPage() {
    // What the original says above its examples: the list belongs to a packaged application.
    return StackPanel {
        InfoBar {Margin {0, 10, 0, 0}, isOpen = true, isClosable = false, severity = InfoBarSeverity::Warning,
                 title = u"JumpList is not available in unpackaged mode.",
                 message = u"This API requires the app to be running in packaged mode (MSIX)."},
        TextBlock {Margin {0, 12, 0, 0}, textWrapping = TextWrapping::Wrap,
                   u"WinUI Gallery populates its jump list with your recently visited and favorited items. Restarting the app will "
                   u"restore these entries after any changes made on this page."},
        tasks(),
        group(),
    };
}
