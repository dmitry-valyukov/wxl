#include "impl/conversions.h"

#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>

#include "Object.impl.h"

// Crossing into WinRT is where an image source stops being text: the
// framework wants an ImageSource object, and the one that loads from a URI
// is a BitmapImage, or an SvgImageSource for a vector image.

namespace wxl::impl {
namespace {

// The application folder as the base URI a path carrying no scheme is
// resolved against.
std::wstring application_folder_uri() {
    return L"file:///" + (core::environment::application_folder() / L"").generic_wstring();
}

}  // namespace

winrt::Windows::Foundation::Uri to_winrt(Uri const& value) {
    if (value.empty()) {
        return nullptr;
    }

    winrt::hstring const& text = to_winrt(value.text());
    auto const absolute = std::wstring_view{text}.find(L"://") != std::wstring_view::npos;
    return absolute ? winrt::Windows::Foundation::Uri{text}
                    : winrt::Windows::Foundation::Uri{application_folder_uri(), text};
}

winrt::Microsoft::UI::Xaml::Media::ImageSource to_winrt(ImageSource const& value) {
    if (value.empty()) {
        return nullptr;
    }
    if (value.bitmap()) {
        return Object::Impl::as<winrt::Microsoft::UI::Xaml::Media::ImageSource>(*value.bitmap());
    }
    // A vector image is not a bitmap: the extension tells which of the two the framework
    // has to be given. The query or fragment of a URL is not part of it.
    auto const uri = to_winrt(value.source());
    auto const owned = uri.Path();
    std::wstring_view const path = owned;
    if (path.size() >= 4 && _wcsicmp(path.data() + path.size() - 4, L".svg") == 0) {
        return winrt::Microsoft::UI::Xaml::Media::Imaging::SvgImageSource{uri};
    }
    return winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage{uri};
}

// An image source that came back from the framework is a real object of
// whatever kind the property was set to; only a bitmap loaded from a URI
// has text to give back, and anything else reads as empty.
ImageSource from_winrt(winrt::Microsoft::UI::Xaml::Media::ImageSource const& value) {
    auto const bitmap = value.try_as<winrt::Microsoft::UI::Xaml::Media::Imaging::BitmapImage>();
    if (!bitmap || !bitmap.UriSource()) {
        return {};
    }
    return ImageSource{Uri{from_winrt(bitmap.UriSource().ToString())}};
}

}  // namespace wxl::impl
