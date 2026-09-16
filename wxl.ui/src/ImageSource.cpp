#include "impl/conversions.h"

#include <windows.h>

#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>

// Crossing into WinRT is where an image source stops being text: the
// framework wants an ImageSource object, and the one that loads from a URI
// is a BitmapImage.

namespace wxl::impl {
namespace {

// The folder the application was started from, as the base URI a path
// carrying no scheme is resolved against. MAX_PATH is not a limit here: the
// buffer grows until the name fits.
std::wstring application_folder() {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        auto const written = ::GetModuleFileNameW(nullptr, path.data(),
                                                  static_cast<DWORD>(path.size()));
        if (written < path.size()) {
            path.resize(written);
            break;
        }
        path.resize(path.size() * 2);
    }
    path.resize(path.find_last_of(L'\\') + 1);
    std::replace(path.begin(), path.end(), L'\\', L'/');
    return L"file:///" + path;
}

}  // namespace

winrt::Microsoft::UI::Xaml::Media::ImageSource to_winrt(ImageSource const& value) {
    if (value.empty()) {
        return nullptr;
    }

    auto const text = value.source().text();
    auto const absolute = text.find(L"://") != std::wstring_view::npos;
    return winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage{
        absolute ? winrt::Windows::Foundation::Uri{text}
                 : winrt::Windows::Foundation::Uri{application_folder(), text}};
}

// An image source that came back from the framework is a real object of
// whatever kind the property was set to; only a bitmap loaded from a URI
// has text to give back, and anything else reads as empty.
ImageSource from_winrt(winrt::Microsoft::UI::Xaml::Media::ImageSource const& value) {
    auto const bitmap = value.try_as<winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage>();
    if (!bitmap || !bitmap.UriSource()) {
        return {};
    }
    return ImageSource{Uri{bitmap.UriSource().ToString()}};
}

}  // namespace wxl::impl
