#pragma once

// wxl::Collection<T> -- the one wrapper for every WinRT vector-shaped
// collection: Panel.Children, ItemsControl.Items, MenuFlyout.Items,
// TabView.TabItems, NavigationView.MenuItems. One hand-written template, not
// one generated wrapper per instantiation.
//
// It is an ordinary member of the wrapper hierarchy -- a wxl::Object holding
// its own Impl -- and it is declared here exactly the way the generated
// wrappers are: member functions declared and not defined, `Impl` named and
// not defined. The definitions live in Collection.impl.h, on the private
// side where winrt:: names are allowed.
//
// Because the bodies are not here, an instantiation cannot appear out of
// nowhere: the header that returns a `Collection<X>` states that the
// specialization exists,
//
//     extern template class Collection<UIElement>;
//
// and exactly one .cpp -- the one where that use lives -- includes
// Collection.impl.h and defines it,
//
//     template class Collection<UIElement>;
//
// That is what keeps the template's code compiled once, inside wxl's own
// build, instead of instantiated afresh in every consuming translation unit.

#include "Object.h"

namespace wxl {

template <typename T>
class Collection : public Object {
    using base_t = Object;

public:
    // Every member is const-qualified, like every other wrapper's: a
    // Collection is a smart pointer to its Impl, so its own constness says
    // nothing about the collection behind it.
    uint32_t size() const;
    bool empty() const;

    T getAt(uint32_t index) const;
    T operator[](uint32_t index) const;

    void setAt(uint32_t index, T const& item) const;
    void insertAt(uint32_t index, T const& item) const;
    void removeAt(uint32_t index) const;
    void append(T const& item) const;
    void removeAtEnd() const;
    void clear() const;

protected:
    class Impl;

    explicit Collection(Impl* impl) noexcept;

    friend class Object::Impl;
};

}  // namespace wxl
