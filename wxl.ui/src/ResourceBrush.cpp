#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.h>

#include "Object.impl.h"
#include "ResourceBrush.h"
#include "generated/Microsoft.UI.Xaml.Media.impl.h"
#include "impl/conversions.h"

namespace wxl {

namespace {

namespace xaml = winrt::Microsoft::UI::Xaml;

// The resource dictionaries of a theme are not in the dictionary's own keys: a color the system theme defines sits in
// ThemeDictionaries under the theme's name, and a library's -- the controls' -- in a merged dictionary, so the search
// goes down through both.
winrt::Windows::Foundation::IInspectable find(xaml::ResourceDictionary const& dictionary, winrt::Windows::Foundation::IInspectable const& key) {
    if (!dictionary) {
        return nullptr;
    }
    // HasKey and Lookup, not TryLookup: the framework answers a missing key with an error, not with nothing.
    if (dictionary.HasKey(key)) {
        return dictionary.Lookup(key);
    }
    if (auto const themes = dictionary.ThemeDictionaries()) {
        for (auto const name : {L"Default", L"Light"}) {
            // TryLookup on a theme dictionary reports a missing theme as an error, not as nothing.
            auto const theme = winrt::box_value(winrt::hstring{name});
            if (themes.HasKey(theme)) {
                if (auto const found = find(themes.Lookup(theme).try_as<xaml::ResourceDictionary>(), key)) {
                    return found;
                }
            }
        }
    }
    auto const merged = dictionary.MergedDictionaries();
    for (uint32_t back = merged.Size(); back != 0; --back) {
        if (auto const found = find(merged.GetAt(back - 1), key)) {
            return found;
        }
    }
    return nullptr;
}

winrt::Windows::Foundation::IInspectable lookup(hstring_param const& key) {
    auto const found = find(xaml::Application::Current().Resources(), winrt::box_value(impl::to_winrt(key)));
    if (!found) {
        throw winrt::hresult_invalid_argument{L"wxl::resourceBrush: no such resource"};
    }
    return found;
}

}  // namespace

Brush resourceBrush(hstring_param const& key) {
    namespace media = winrt::Microsoft::UI::Xaml::Media;

    auto const value = lookup(key);
    if (auto const color = value.try_as<winrt::Windows::UI::Color>()) {
        return Object::Impl::wrap<Brush>(media::Brush{media::SolidColorBrush{*color}});
    }
    return Object::Impl::wrap<Brush>(value.as<media::Brush>());
}

Color resourceColor(hstring_param const& key) {
    return impl::from_winrt(winrt::unbox_value<winrt::Windows::UI::Color>(lookup(key)));
}

}  // namespace wxl
