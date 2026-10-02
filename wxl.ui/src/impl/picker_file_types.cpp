#include "picker_file_types.h"

#include "conversions.h"

namespace wxl::impl {

void add_file_type(winrt::Microsoft::Windows::Storage::Pickers::FileOpenPicker const& picker, hstring_param const& extension) {
    picker.FileTypeFilter().Append(to_winrt(extension));
}

void add_file_type_choice(winrt::Microsoft::Windows::Storage::Pickers::FileSavePicker const& picker, FileTypeChoice const& choice) {
    std::vector<winrt::hstring> extensions;
    extensions.reserve(choice.extensions.size());
    for (auto const& extension : choice.extensions) extensions.push_back(winrt::hstring {std::wstring_view {reinterpret_cast<wchar_t const*>(extension.data()), extension.size()}});
    picker.FileTypeChoices().Insert(winrt::hstring {std::wstring_view {reinterpret_cast<wchar_t const*>(choice.description.data()), choice.description.size()}}, winrt::single_threaded_vector(std::move(extensions)));
}

}  // namespace wxl::impl
