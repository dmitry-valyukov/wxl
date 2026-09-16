#pragma once

// wxl::Rows / wxl::Columns -- the stack that deals its space out evenly.
//
// A vertical StackPanel gives every child exactly its desired height and can
// do nothing else: children pinned to an edge or stretched over the free
// space are Grid territory, at the price of declaring the definitions and
// numbering a row onto every child by hand. These two keep the Grid and drop
// the bookkeeping: each child dropped into the braces claims a fresh star
// row (column) of its own, so the panel's extent is dealt out in equal
// shares and the declaration reads like a StackPanel.
//
// Distinct types rather than a mode on Grid on purpose. A Grid child with no
// row is XAML's own way of saying "row 0", and a Grid that quietly renumbered
// children by their position would hand that one written form a second
// meaning; a different type says the different meaning out loud. Everything
// of Grid's is still here -- rowSpacing above all -- because a Rows *is* a
// Grid.

#include "generated/Microsoft.UI.Xaml.Controls.h"

namespace wxl {

class Rows : public Grid {
public:
    Rows() = default;

    template <typename... Setters>
        requires impl::setter_pack<Rows, Setters...>
    explicit Rows(Setters&&... setters) {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    // A child claims the next row. The star height is what makes the shares
    // equal; the base overload this hides would have piled everyone into
    // row 0. Within its share the child still obeys its own alignment --
    // stretched by default, like any Grid cell.
    using Grid::setPositional;
    void setPositional(FrameworkElement const& child) const {
        RowDefinition share;
        share.height({1.0, GridUnitType::Star});
        rowDefinitions().append(share);
        setRow(child, static_cast<int32_t>(rowDefinitions().size()) - 1);
        children().append(child);
    }

protected:
    // What try_as builds the wrapper through, and the reason a hand-written
    // class needs it spelled out: every generated wrapper has one, and
    // without it a handler could not name this type as its sender.
    explicit Rows(Impl* impl) noexcept : Grid{impl} {}

    friend class Object::Impl;
};

class Columns : public Grid {
public:
    Columns() = default;

    template <typename... Setters>
        requires impl::setter_pack<Columns, Setters...>
    explicit Columns(Setters&&... setters) {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    using Grid::setPositional;
    void setPositional(FrameworkElement const& child) const {
        ColumnDefinition share;
        share.width({1.0, GridUnitType::Star});
        columnDefinitions().append(share);
        setColumn(child, static_cast<int32_t>(columnDefinitions().size()) - 1);
        children().append(child);
    }

protected:
    explicit Columns(Impl* impl) noexcept : Grid{impl} {}

    friend class Object::Impl;
};

// Their bodies live in Panels.cpp: a handler may name either as its sender,
// and only there is try_as defined.
extern template Rows Object::try_as<Rows>() const;
extern template Columns Object::try_as<Columns>() const;

}  // namespace wxl
