#pragma once

// impl::element_scope -- the bindings one element of a list made while it was built, kept
// where the list machinery knows when the element goes.
//
// A list control builds an element each time an item comes into view, and every binding
// the builder writes is a watch its field keeps, owning the control it writes to. Built
// inside the scope (core::binding_scope::collect), those watches are collected here too,
// and cut() takes them off their fields when the control gives the element back: a
// ListView container going to its recycle queue, an ItemsRepeater element handed to
// RecycleElement. Dropped, the scope cuts them as well.
//
// It is a COM object so that a ListView container can carry it -- in the Tag of the place
// the element is shown in, which wxl made and no application sees -- and the scope then
// lives and dies with the container.

#include "sta_com.h"

namespace wxl::impl {

class element_scope final : public sta_com_object<element_scope, winrt::Windows::Foundation::IInspectable>
{
public:
    static constexpr winrt::guid own_iid{0x911dcc9a, 0x2056, 0x4b81, {0x84, 0x2a, 0x4e, 0x82, 0x72, 0x39, 0x7e, 0x35}};

    core::binding_scope bindings;
};

}  // namespace wxl::impl
