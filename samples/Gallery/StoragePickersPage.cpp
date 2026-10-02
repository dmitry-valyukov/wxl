// Страница StoragePickers -- StoragePickersPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <cmath>
#include "generated/Windows.Storage.h"
#include "generated/Windows.Storage.FileProperties.h"
#include "generated/Microsoft.Windows.Storage.Pickers.h"
#include "PickerFileTypes.h"
#include "generated/Microsoft.UI.Content.h"

#include "generated/Microsoft.UI.Xaml.Media.Imaging.h"

import wxl.async;

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t singleHeader[] = {
#include "Snippets/StoragePickers/StoragePickersPickSingleFile.html.embed"
};
constexpr char8_t singleCode[] = {
#include "Snippets/StoragePickers/StoragePickersPickSingleFile.h.embed"
};

FrameworkElement single() {
#include "Snippets/StoragePickers/StoragePickersPickSingleFile.h"

    return gallery::controlExample({
        .header = gallery::snippet(singleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(singleCode),
    });
}

constexpr char8_t multipleHeader[] = {
#include "Snippets/StoragePickers/StoragePickersPickMultipleFiles.html.embed"
};
constexpr char8_t multipleCode[] = {
#include "Snippets/StoragePickers/StoragePickersPickMultipleFiles.h.embed"
};

FrameworkElement multiple() {
#include "Snippets/StoragePickers/StoragePickersPickMultipleFiles.h"

    return gallery::controlExample({
        .header = gallery::snippet(multipleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(multipleCode),
    });
}

constexpr char8_t saveHeader[] = {
#include "Snippets/StoragePickers/StoragePickersSaveFile.html.embed"
};
constexpr char8_t saveCode[] = {
#include "Snippets/StoragePickers/StoragePickersSaveFile.h.embed"
};

FrameworkElement save() {
#include "Snippets/StoragePickers/StoragePickersSaveFile.h"

    return gallery::controlExample({
        .header = gallery::snippet(saveHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(saveCode),
    });
}

constexpr char8_t folderHeader[] = {
#include "Snippets/StoragePickers/StoragePickersPickFolder.html.embed"
};
constexpr char8_t folderCode[] = {
#include "Snippets/StoragePickers/StoragePickersPickFolder.h.embed"
};

FrameworkElement folder() {
#include "Snippets/StoragePickers/StoragePickersPickFolder.h"

    return gallery::controlExample({
        .header = gallery::snippet(folderHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(folderCode),
    });
}

constexpr char8_t thumbnailHeader[] = {
#include "Snippets/StoragePickers/StoragePickersFileThumbnail.html.embed"
};
constexpr char8_t thumbnailCode[] = {
#include "Snippets/StoragePickers/StoragePickersFileThumbnail.h.embed"
};

FrameworkElement thumbnail() {
#include "Snippets/StoragePickers/StoragePickersFileThumbnail.h"

    return gallery::controlExample({
        .header = gallery::snippet(thumbnailHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(thumbnailCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::storagePickersPage() {
    return StackPanel {single(), multiple(), save(), folder(), thumbnail()};
}
