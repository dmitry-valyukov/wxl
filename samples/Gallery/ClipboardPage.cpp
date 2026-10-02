// Страница Clipboard -- ClipboardPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include "ApplicationFolder.h"
#include "generated/Windows.ApplicationModel.DataTransfer.h"
#include "generated/Windows.Storage.h"
#include "generated/Windows.Storage.Streams.h"
#include "generated/Microsoft.Windows.Storage.Pickers.h"
#include "generated/Microsoft.UI.Content.h"

#include "generated/Microsoft.UI.Xaml.Media.Imaging.h"

import wxl.async;

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t copyTextHeader[] = {
#include "Snippets/Clipboard/CopyTextClipboard.html.embed"
};
constexpr char8_t copyTextCode[] = {
#include "Snippets/Clipboard/CopyTextClipboard.h.embed"
};

FrameworkElement copyText() {
#include "Snippets/Clipboard/CopyTextClipboard.h"

    return gallery::controlExample({
        .header = gallery::snippet(copyTextHeader),
        .example = example,
        .code = gallery::snippet(copyTextCode),
    });
}

constexpr char8_t pasteTextHeader[] = {
#include "Snippets/Clipboard/PasteTextClipboard.html.embed"
};
constexpr char8_t pasteTextCode[] = {
#include "Snippets/Clipboard/PasteTextClipboard.h.embed"
};

FrameworkElement pasteText() {
#include "Snippets/Clipboard/PasteTextClipboard.h"

    return gallery::controlExample({
        .header = gallery::snippet(pasteTextHeader),
        .example = example,
        .code = gallery::snippet(pasteTextCode),
    });
}

constexpr char8_t imageHeader[] = {
#include "Snippets/Clipboard/ClipboardCopyPasteImage.html.embed"
};
constexpr char8_t imageCode[] = {
#include "Snippets/Clipboard/ClipboardCopyPasteImage.h.embed"
};

FrameworkElement image() {
#include "Snippets/Clipboard/ClipboardCopyPasteImage.h"

    return gallery::controlExample({
        .header = gallery::snippet(imageHeader),
        .example = example,
        .code = gallery::snippet(imageCode),
    });
}

constexpr char8_t filesHeader[] = {
#include "Snippets/Clipboard/ClipboardCopyPasteFiles.html.embed"
};
constexpr char8_t filesCode[] = {
#include "Snippets/Clipboard/ClipboardCopyPasteFiles.h.embed"
};

FrameworkElement files() {
#include "Snippets/Clipboard/ClipboardCopyPasteFiles.h"

    return gallery::controlExample({
        .header = gallery::snippet(filesHeader),
        .example = example,
        .code = gallery::snippet(filesCode),
    });
}

constexpr char8_t optionsHeader[] = {
#include "Snippets/Clipboard/ClipboardHistoryRoamingOptions.html.embed"
};
constexpr char8_t optionsCode[] = {
#include "Snippets/Clipboard/ClipboardHistoryRoamingOptions.h.embed"
};

FrameworkElement options() {
#include "Snippets/Clipboard/ClipboardHistoryRoamingOptions.h"

    return gallery::controlExample({
        .header = gallery::snippet(optionsHeader),
        .example = example,
        .code = gallery::snippet(optionsCode),
    });
}

constexpr char8_t otherHeader[] = {
#include "Snippets/Clipboard/OtherClipboardOperations.html.embed"
};
constexpr char8_t otherCode[] = {
#include "Snippets/Clipboard/OtherClipboardOperations.h.embed"
};

FrameworkElement other() {
#include "Snippets/Clipboard/OtherClipboardOperations.h"

    return gallery::controlExample({
        .header = gallery::snippet(otherHeader),
        .example = example,
        .code = gallery::snippet(otherCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::clipboardPage() {
    return StackPanel {copyText(), pasteText(), image(), files(), options(), other()};
}
