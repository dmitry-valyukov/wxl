#pragma once

// The private side of wxl::VectorView<T>: the Impl holding the WinRT view,
// and the member definitions. Included only by the .cpp that explicitly
// instantiates a given specialization, never by consuming code.

#include <winrt/Windows.Foundation.Collections.h>

#include "VectorView.h"
#include "impl/vector_element.h"

namespace wxl {

template <typename T>
class VectorView<T>::Impl : public Object::Impl {
public:
    using element = impl::vector_element<T>;
    using view_t = winrt::Windows::Foundation::Collections::IVectorView<typename element::winrt_t>;

    using Object::Impl::Impl;

    // A view arrives from the member that returned it, typed: it is filled
    // here as it is, instead of costing a QueryInterface on first use the way
    // an ordinary interface field does.
    template <typename Source>
        requires std::constructible_from<view_t, Source const&>
    explicit Impl(Source&& source) : Object::Impl(source), view_(std::forward<Source>(source)) {}

    view_t view_;

    // A view stands for the interface it holds, so that is what it hands to a
    // call.
    using winrt_t = view_t;

    operator view_t const&() { return get<&Impl::view_>(); }
};

template <typename T>
VectorView<T>::VectorView(Impl* impl) noexcept : base_t(impl) {}

template <typename T>
uint32_t VectorView<T>::size() const {
    return get<&Impl::view_>().Size();
}

template <typename T>
bool VectorView<T>::empty() const {
    return size() == 0;
}

template <typename T>
T VectorView<T>::getAt(uint32_t index) const {
    return Impl::element::from(get<&Impl::view_>().GetAt(index));
}

template <typename T>
T VectorView<T>::operator[](uint32_t index) const {
    return getAt(index);
}

template <typename T>
core::nullable<uint32_t> VectorView<T>::indexOf(T const& value) const {
    uint32_t index = 0;
    if (get<&Impl::view_>().IndexOf(Impl::element::to(value), index)) return index;
    return {};
}

}  // namespace wxl
