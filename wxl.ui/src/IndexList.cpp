#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include "IndexList.h"

namespace wxl {

namespace {

using winrt::Windows::Foundation::IInspectable;
using vector_t = winrt::Windows::Foundation::Collections::IVector<IInspectable>;

Object object_of(vector_t const& list) {
    return Object::copy_from_abi(static_cast<::IInspectable*>(winrt::get_abi(list)));
}

vector_t vector_of(Object const& list) {
    IInspectable value{nullptr};
    winrt::copy_from_abi(value, list.get_abi());
    return value.as<vector_t>();
}

Object counted(int64_t count) {
    auto list = winrt::single_threaded_vector<IInspectable>();
    for (int64_t index = 0; index < count; ++index) list.Append(winrt::box_value(index));
    return object_of(list);
}

Object picked(std::span<int64_t const> positions) {
    auto list = winrt::single_threaded_vector<IInspectable>();
    for (int64_t const position : positions) list.Append(winrt::box_value(position));
    return object_of(list);
}

}  // namespace

IndexList::IndexList(int64_t count) : Object(counted(count)) {}

IndexList::IndexList(std::span<int64_t const> positions) : Object(picked(positions)) {}

IndexList::IndexList(Object const& list) : Object(list) {
    // Asked once, here: an object that is not a vector of objects made into an IndexList would
    // fail at the first call instead, far from where it was given.
    IInspectable value{nullptr};
    winrt::copy_from_abi(value, list.get_abi());
    if (!value.try_as<vector_t>())
        core::abort("wxl: IndexList over an object that is not a vector of objects");
}

IndexList IndexList::observable() {
    return IndexList{object_of(winrt::single_threaded_observable_vector<IInspectable>())};
}

void IndexList::append(int64_t value) const {
    vector_of(*this).Append(winrt::box_value(value));
}

void IndexList::insertAt(uint32_t index, int64_t value) const {
    vector_of(*this).InsertAt(index, winrt::box_value(value));
}

void IndexList::removeAt(uint32_t index) const {
    vector_of(*this).RemoveAt(index);
}

int64_t IndexList::getAt(uint32_t index) const {
    return winrt::unbox_value<int64_t>(vector_of(*this).GetAt(index));
}

Object IndexList::objectAt(uint32_t index) const {
    auto const item = vector_of(*this).GetAt(index);
    return Object::copy_from_abi(static_cast<::IInspectable*>(winrt::get_abi(item)));
}

uint32_t IndexList::size() const {
    return vector_of(*this).Size();
}

}  // namespace wxl
