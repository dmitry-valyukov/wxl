#include "resource.h"

#include "conversions.h"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

namespace wxl::impl {

namespace {

void insert(winrt::Microsoft::UI::Xaml::ResourceDictionary const& dictionary, Resource const& entry) {
    winrt::Windows::Foundation::IInspectable boxed = std::visit(
        [](auto const& value) -> winrt::Windows::Foundation::IInspectable {
            using type = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::same_as<type, bool> || std::same_as<type, double>) {
                return winrt::box_value(value);
            } else if constexpr (std::same_as<type, HorizontalAlignment>) {
                return winrt::box_value(static_cast<winrt::Microsoft::UI::Xaml::HorizontalAlignment>(value));
            } else if constexpr (std::same_as<type, Object>) {
                winrt::Windows::Foundation::IInspectable native{nullptr};
                winrt::copy_from_abi(native, value.get_abi());
                return native;
            } else {
                return winrt::box_value(to_winrt(value));
            }
        },
        entry.value);
    dictionary.Insert(winrt::box_value(to_winrt(entry.key)), boxed);
}

}  // namespace

void add_resource(winrt::Microsoft::UI::Xaml::ResourceDictionary const& dictionary, Resource const& entry) {
    insert(dictionary, entry);
}

void add_theme_resources(winrt::Microsoft::UI::Xaml::ResourceDictionary const& dictionary, ThemeResources const& theme) {
    winrt::Microsoft::UI::Xaml::ResourceDictionary inner;
    for (auto const& entry : theme.entries) {
        insert(inner, entry);
    }
    dictionary.ThemeDictionaries().Insert(winrt::box_value(to_winrt(theme.theme)), inner);
}

}  // namespace wxl::impl
