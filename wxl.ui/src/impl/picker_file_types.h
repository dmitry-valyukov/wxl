#pragma once

#include <winrt/Microsoft.Windows.Storage.Pickers.h>

#include "../PickerFileTypes.h"
#include "../hstring_param.h"

// What stands behind `fileType = u".txt"` on an open picker and `fileTypeChoice = ...` on a save picker:
// one entry appended to the picker's own list.
//
// Private: the picker arrives as the projection type the wrapper already holds, so
// this header is one only wxl's own sources ever include.

namespace wxl::impl {

void add_file_type(winrt::Microsoft::Windows::Storage::Pickers::FileOpenPicker const& picker, hstring_param const& extension);
void add_file_type_choice(winrt::Microsoft::Windows::Storage::Pickers::FileSavePicker const& picker, FileTypeChoice const& choice);

}  // namespace wxl::impl
