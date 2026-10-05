#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include "StringList.h"
#include "impl/conversions.h"

namespace wxl {

Object stringList(std::span<std::u16string const> items) {
    using winrt::Windows::Foundation::IInspectable;

    auto list = winrt::single_threaded_vector<IInspectable>();
    for (auto const& item : items) {
        // The box keeps a string of its own, so the text is copied once, into
        // it -- from a header over the caller's characters, not through an
        // hstring made first.
        list.Append(impl::box_text(item));
    }
    return Object::copy_from_abi(static_cast<::IInspectable*>(winrt::get_abi(list)));
}

Object objectList(std::span<Object const> items) {
    using winrt::Windows::Foundation::IInspectable;

    auto list = winrt::single_threaded_vector<IInspectable>();
    for (auto const& item : items) {
        IInspectable value{nullptr};
        winrt::copy_from_abi(value, item.get_abi());
        list.Append(value);
    }
    return Object::copy_from_abi(static_cast<::IInspectable*>(winrt::get_abi(list)));
}

Object stringBox(hstring_param const& text) {
    auto const box = impl::box_text(text);
    return Object::copy_from_abi(static_cast<::IInspectable*>(winrt::get_abi(box)));
}

hstring stringOf(Object const& boxed) {
    winrt::Windows::Foundation::IInspectable value{nullptr};
    winrt::copy_from_abi(value, boxed.get_abi());

    return impl::from_winrt(winrt::unbox_value_or<winrt::hstring>(value, winrt::hstring{}));
}

}  // namespace wxl
