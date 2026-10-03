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

Object indexList(int64_t const* positions, size_t count) {
    auto list = winrt::single_threaded_vector<winrt::Windows::Foundation::IInspectable>();
    for (size_t i = 0; i < count; ++i) {
        list.Append(winrt::box_value(positions[i]));
    }
    return Object::copy_from_abi(static_cast<::IInspectable*>(winrt::get_abi(list)));
}

int64_t intOf(Object const& boxed) {
    winrt::Windows::Foundation::IInspectable value{nullptr};
    winrt::copy_from_abi(value, boxed.get_abi());
    return winrt::unbox_value_or<int64_t>(value, -1);
}

Object observableIndexList() {
    auto list = winrt::single_threaded_observable_vector<winrt::Windows::Foundation::IInspectable>();
    return Object::copy_from_abi(static_cast<::IInspectable*>(winrt::get_abi(list)));
}

namespace {

winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Foundation::IInspectable> vector_of(Object const& list) {
    winrt::Windows::Foundation::IInspectable value{nullptr};
    winrt::copy_from_abi(value, list.get_abi());
    return value.as<winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Foundation::IInspectable>>();
}

}  // namespace

void indexListAppend(Object const& list, int64_t value) {
    vector_of(list).Append(winrt::box_value(value));
}

void indexListRemoveAt(Object const& list, uint32_t index) {
    vector_of(list).RemoveAt(index);
}

uint32_t indexListSize(Object const& list) {
    return vector_of(list).Size();
}

}  // namespace wxl
