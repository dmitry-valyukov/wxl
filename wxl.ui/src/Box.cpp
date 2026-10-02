#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include "Box.h"

namespace wxl {

Object intBox(int64_t value) {
    auto const box = winrt::box_value(value);
    return Object::copy_from_abi(static_cast<::IInspectable*>(winrt::get_abi(box)));
}

Object indexList(int64_t count) {
    auto list = winrt::single_threaded_vector<winrt::Windows::Foundation::IInspectable>();
    for (int64_t index = 0; index < count; ++index) {
        list.Append(winrt::box_value(index));
    }
    return Object::copy_from_abi(static_cast<::IInspectable*>(winrt::get_abi(list)));
}

int64_t intOf(Object const& boxed) {
    winrt::Windows::Foundation::IInspectable value{nullptr};
    winrt::copy_from_abi(value, boxed.get_abi());
    return winrt::unbox_value_or<int64_t>(value, -1);
}

}  // namespace wxl
