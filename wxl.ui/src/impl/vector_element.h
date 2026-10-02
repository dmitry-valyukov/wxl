#pragma once

// How an element crosses between a WinRT list and wxl, for both templates that
// hold one: Collection<T> over an IVector, VectorView<T> over an IVectorView.
//
// `winrt_t` is the element's type on the WinRT side, and it has to be the real
// one: a parameterized interface has a different IID for every argument, so
// an IVector<UIElement> does not answer to IVector<IInspectable>. `from` turns
// an element read off the list into T, `to` turns a T into what the list
// takes.
//
// Private: names winrt:: types.

#include <winrt/Windows.Foundation.Collections.h>

#include "../Object.impl.h"
#include "../iterable_param.h"
#include "conversions.h"

namespace wxl::impl {

// A value wxl projects onto a type of its own -- a string, a Uri, a Point --
// crosses through the to_winrt / from_winrt overloads of conversions.h, and
// its WinRT type is whatever to_winrt makes of it. Only those overloads: the
// call below is looked up where this template is defined, and an overload
// declared later in namespace impl (a generated struct's, in Structs.impl.h)
// is out of its sight.
template <typename T>
struct vector_element {
    using winrt_t = std::remove_cvref_t<decltype(to_winrt(std::declval<T const&>()))>;

    static T from(winrt_t const& value) { return from_winrt(value); }
    static decltype(auto) to(T const& value) { return to_winrt(value); }
};

// A number is the same type on both sides.
template <typename T>
    requires std::is_arithmetic_v<T>
struct vector_element<T> {
    using winrt_t = T;

    static T from(T value) noexcept { return value; }
    static T to(T value) noexcept { return value; }
};

// A wrapper stands for the WinRT type its own Impl says it does
// (Object::Impl::winrt_type), so nothing here has to be told what T is and
// there is no second table to keep in step with the generated one. Crossing
// out builds a wrapper around the element; crossing in hands over the
// element's own Impl, which converts to the interface it holds.
template <typename T>
    requires std::derived_from<T, Object>
struct vector_element<T> {
    using winrt_t = Object::Impl::winrt_type<T>;

    static T from(winrt_t value) { return Object::Impl::wrap<T>(std::move(value)); }
    static winrt_t const& to(T const& value) { return *Object::Impl::get_typed<T>(value); }
};

// The sequence an IIterable<T> parameter is given. A list wxl holds is the
// very object behind it, asked for IIterable<T> -- which a vector and a view
// both implement; elements in memory go into a WinRT vector made for the
// call, there being no object to lend.
template <typename T>
winrt::Windows::Foundation::Collections::IIterable<typename vector_element<T>::winrt_t> to_winrt(
    iterable_param<T> const& value) {
    using item_t = typename vector_element<T>::winrt_t;
    if (auto const* list = value.list()) {
        return Object::Impl::get_typed(*list)
            ->inspectable_.template as<winrt::Windows::Foundation::Collections::IIterable<item_t>>();
    }

    std::vector<item_t> items;
    items.reserve(value.items().size());
    for (auto const& item : value.items()) {
        items.push_back(vector_element<T>::to(item));
    }
    return winrt::single_threaded_vector<item_t>(std::move(items));
}

}  // namespace wxl::impl
