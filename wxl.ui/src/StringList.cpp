#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include "StringList.h"

namespace wxl {

Object stringList(std::span<std::u16string const> items) {
    using winrt::Windows::Foundation::IInspectable;

    auto list = winrt::single_threaded_vector<IInspectable>();
    for (auto const& item : items) {
        list.Append(winrt::box_value(
            winrt::hstring{reinterpret_cast<wchar_t const*>(item.data()),
                           static_cast<uint32_t>(item.size())}));
    }
    return Object::copy_from_abi(static_cast<::IInspectable*>(winrt::get_abi(list)));
}

std::u16string stringOf(Object const& boxed) {
    winrt::Windows::Foundation::IInspectable value{nullptr};
    winrt::copy_from_abi(value, boxed.get_abi());

    auto const text = winrt::unbox_value_or<winrt::hstring>(value, winrt::hstring{});
    return std::u16string{reinterpret_cast<char16_t const*>(text.c_str()), text.size()};
}

}  // namespace wxl
