#include "resource.h"

#include "conversions.h"

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

namespace wxl::impl {

void add_resource(winrt::Microsoft::UI::Xaml::ResourceDictionary const& dictionary, Resource const& entry) {
    winrt::Windows::Foundation::IInspectable boxed = std::visit(
        [](auto const& value) -> winrt::Windows::Foundation::IInspectable {
            using type = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::same_as<type, bool> || std::same_as<type, double>) {
                return winrt::box_value(value);
            } else {
                return winrt::box_value(to_winrt(value));
            }
        },
        entry.value);
    dictionary.Insert(winrt::box_value(to_winrt(entry.key)), boxed);
}

}  // namespace wxl::impl
