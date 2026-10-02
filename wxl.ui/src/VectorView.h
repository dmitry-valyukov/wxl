#pragma once

// wxl::VectorView<T> -- a WinRT IVectorView<T>: a list a call hands back to
// be read and not changed, held as the interface itself.
//
// Nothing is copied. Every read goes to the object behind the view, so
// whatever that object does the view does too: a view the runtime keeps in
// step with its source shows each change as it happens, and a snapshot stays
// a snapshot. Which of the two a view is, is up to whoever handed it out --
// copying it here would have made every view a snapshot.
//
// It is a wrapper like any other -- a wxl::Object holding its own Impl -- and
// it is declared the way Collection<T> is, for the same reason: members
// declared and not defined, `Impl` named and not defined, the definitions in
// VectorView.impl.h on the private side, where winrt:: names are allowed. A
// header that hands out a VectorView<X> states that the specialization
// exists,
//
//     extern template class VectorView<hstring>;
//
// and exactly one .cpp includes VectorView.impl.h and defines it, so the
// template's code is compiled once, inside wxl's own build.
//
// T is a wrapper, a number, or a type that crosses through
// impl/conversions.h -- a string, a Uri, a Point (see impl/vector_element.h).

#include "Object.h"
#include "impl/index_iterator.h"

namespace wxl {

template <typename T>
class VectorView : public Object {
    using base_t = Object;

public:
    // Const-qualified, like every member of every wrapper: a VectorView is a
    // smart pointer to its Impl.
    uint32_t size() const;
    bool empty() const;

    T getAt(uint32_t index) const;
    T operator[](uint32_t index) const;

    // Where the first element equal to `value` is, or nothing.
    core::nullable<uint32_t> indexOf(T const& value) const;

    impl::index_iterator<VectorView> begin() const noexcept { return {this, 0}; }
    impl::index_iterator<VectorView> end() const { return {this, size()}; }

protected:
    class Impl;

    explicit VectorView(Impl* impl) noexcept;

    friend class Object::Impl;
};

}  // namespace wxl
