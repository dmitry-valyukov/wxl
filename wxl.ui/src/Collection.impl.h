#pragma once

// The private side of wxl::Collection<T>: the Impl holding the WinRT vector,
// and the member definitions. Included only by the .cpp that explicitly
// instantiates a given specialization, never by consuming code.

#include <winrt/Windows.Foundation.Collections.h>

#include "Collection.h"
#include "impl/vector_element.h"

namespace wxl {

// How an element crosses -- and which WinRT type the vector is of -- is
// impl::vector_element's, shared with VectorView<T>.

template <typename T>
class Collection<T>::Impl : public Object::Impl {
public:
    using element = impl::vector_element<T>;
    using vector_t = winrt::Windows::Foundation::Collections::IVector<typename element::winrt_t>;

    using Object::Impl::Impl;

    // A collection never arrives as a bare IInspectable: the member handing
    // it over has the concrete type in hand -- UIElementCollection, which the
    // projection derives from IVector<UIElement> -- so the vector is filled
    // here, as a base-class conversion, instead of costing a QueryInterface
    // on first use the way an ordinary interface field does.
    template <typename Source>
        requires std::constructible_from<vector_t, Source const&>
    explicit Impl(Source&& source)
        : Object::Impl(source), vector_(std::forward<Source>(source)) {}

    vector_t vector_;

    // A collection stands for the vector it holds, so that is what it hands
    // to a call.
    using winrt_t = vector_t;

    operator vector_t const&() { return get<&Impl::vector_>(); }
};

template <typename T>
Collection<T>::Collection(Impl* impl) noexcept : base_t(impl) {}

template <typename T>
uint32_t Collection<T>::size() const {
    return get<&Impl::vector_>().Size();
}

template <typename T>
bool Collection<T>::empty() const {
    return size() == 0;
}

template <typename T>
T Collection<T>::getAt(uint32_t index) const {
    return Impl::element::from(get<&Impl::vector_>().GetAt(index));
}

template <typename T>
T Collection<T>::operator[](uint32_t index) const {
    return getAt(index);
}

template <typename T>
void Collection<T>::setAt(uint32_t index, T const& item) const {
    get<&Impl::vector_>().SetAt(index, Impl::element::to(item));
}

template <typename T>
void Collection<T>::insertAt(uint32_t index, T const& item) const {
    get<&Impl::vector_>().InsertAt(index, Impl::element::to(item));
}

template <typename T>
void Collection<T>::removeAt(uint32_t index) const {
    get<&Impl::vector_>().RemoveAt(index);
}

template <typename T>
void Collection<T>::append(T const& item) const {
    get<&Impl::vector_>().Append(Impl::element::to(item));
}

template <typename T>
void Collection<T>::removeAtEnd() const {
    get<&Impl::vector_>().RemoveAtEnd();
}

template <typename T>
void Collection<T>::clear() const {
    get<&Impl::vector_>().Clear();
}

}  // namespace wxl
