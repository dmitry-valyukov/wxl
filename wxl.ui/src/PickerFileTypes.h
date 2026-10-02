#pragma once

#include <string>
#include <vector>

// What the file pickers are told about file types, in wxl's terms.
//
// The pickers keep the types in a WinRT vector (FileTypeFilter) and a WinRT map of
// vectors (FileTypeChoices) that the application fills one entry at a time:
//
//     open.fileType(u".txt");
//     save.fileTypeChoice(FileTypeChoice {u"Text Files", {u".txt"}});

namespace wxl {

/// One line of the "Save as type" list of a save dialog: the words the user reads and the
/// extensions that stand behind them.
struct FileTypeChoice {
    std::u16string description;
    std::vector<std::u16string> extensions;
};

}  // namespace wxl
