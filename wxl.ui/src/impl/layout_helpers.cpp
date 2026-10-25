// The projection comes first, and with it every standard header it needs:
// wxl's own headers carry the wxl.core import, and a standard header included
// after that import is one the compiler has already seen through the std
// module.
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.UI.Xaml.Interop.h>

#include "layout_helpers.h"

#include "conversions.h"

#include "../Object.impl.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>
#include <wxl/Structs.impl.h>

// Imports last, after every plain header.
import wxl.core;

namespace wxl::impl {

namespace {

// The one building of a grid's definitions, whether the lengths were parsed from a text or
// computed: the old ones go, and one definition per length is appended in order.
void build_rows(winrt::Microsoft::UI::Xaml::Controls::Grid const& grid, std::span<GridLength const> lengths) {
    auto const rows = grid.RowDefinitions();
    rows.Clear();
    for (GridLength const length : lengths) {
        winrt::Microsoft::UI::Xaml::Controls::RowDefinition row;
        row.Height(to_winrt(length));
        rows.Append(row);
    }
}

void build_columns(winrt::Microsoft::UI::Xaml::Controls::Grid const& grid, std::span<GridLength const> lengths) {
    auto const columns = grid.ColumnDefinitions();
    columns.Clear();
    for (GridLength const length : lengths) {
        winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition column;
        column.Width(to_winrt(length));
        columns.Append(column);
    }
}

}  // namespace

void set_row_definitions(winrt::Microsoft::UI::Xaml::Controls::Grid const& grid, RowLengths const& rows) {
    build_rows(grid, rows.lengths());
}

void set_column_definitions(winrt::Microsoft::UI::Xaml::Controls::Grid const& grid,
                            ColumnLengths const& columns) {
    build_columns(grid, columns.lengths());
}

void set_row_definitions(Grid const& grid, std::span<GridLength const> lengths) {
    build_rows(*Object::Impl::get_typed<Grid>(grid), lengths);
}

void set_column_definitions(Grid const& grid, std::span<GridLength const> lengths) {
    build_columns(*Object::Impl::get_typed<Grid>(grid), lengths);
}

namespace {

namespace controls = winrt::Microsoft::UI::Xaml::Controls;

controls::AnnotatedScrollBar native(AnnotatedScrollBar const& bar) {
    return *Object::Impl::get_typed<AnnotatedScrollBar>(bar);
}

// A ScrollView has its ScrollPresenter once its template is applied, which is
// when it is loaded: before that the presenter is null and there is nothing to
// give a controller to. So a view not yet loaded is given the controller at
// its Loaded, once -- the handler takes itself off, and with it lets go of the
// bar.
template <typename Give>
void give_when_loaded(controls::ScrollView const& view, controls::AnnotatedScrollBar const& bar, Give give) {
    if (auto const presenter = view.ScrollPresenter()) {
        give(presenter, bar);
        return;
    }
    auto const token = core::make_refcounted<winrt::event_token>();
    *token = view.Loaded([bar, give, token](winrt::Windows::Foundation::IInspectable const& sender,
                                            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&) {
        auto const loaded = sender.as<controls::ScrollView>();
        give(loaded.ScrollPresenter(), bar);
        loaded.Loaded(*token);
    });
}

}  // namespace

void set_vertical_scroll_controller(controls::ScrollView const& view, AnnotatedScrollBar const& bar) {
    give_when_loaded(view, native(bar), [](auto const& presenter, auto const& scrollBar) {
        presenter.VerticalScrollController(scrollBar.ScrollController());
    });
}

void set_horizontal_scroll_controller(controls::ScrollView const& view, AnnotatedScrollBar const& bar) {
    give_when_loaded(view, native(bar), [](auto const& presenter, auto const& scrollBar) {
        presenter.HorizontalScrollController(scrollBar.ScrollController());
    });
}

namespace {

winrt::Microsoft::UI::Xaml::Media::PointCollection points_from_text(hstring_param const& text) {
    using winrt::Windows::UI::Xaml::Interop::TypeKind;
    using winrt::Windows::UI::Xaml::Interop::TypeName;

    return winrt::Microsoft::UI::Xaml::Markup::XamlBindingHelper::ConvertValue(
               TypeName{L"Microsoft.UI.Xaml.Media.PointCollection", TypeKind::Metadata},
               winrt::box_value(to_winrt(text)))
        .as<winrt::Microsoft::UI::Xaml::Media::PointCollection>();
}

}  // namespace

void set_points(winrt::Microsoft::UI::Xaml::Shapes::Polyline const& shape, hstring_param const& text) {
    shape.Points(points_from_text(text));
}

void set_points(winrt::Microsoft::UI::Xaml::Shapes::Polygon const& shape, hstring_param const& text) {
    shape.Points(points_from_text(text));
}

winrt::Microsoft::UI::Xaml::Media::Geometry geometry_from_text(hstring_param const& text) {
    using winrt::Microsoft::UI::Xaml::Markup::XamlBindingHelper;
    using winrt::Windows::UI::Xaml::Interop::TypeKind;
    using winrt::Windows::UI::Xaml::Interop::TypeName;

    return XamlBindingHelper::ConvertValue(TypeName{L"Microsoft.UI.Xaml.Media.Geometry", TypeKind::Metadata},
                                           winrt::box_value(to_winrt(text)))
        .as<winrt::Microsoft::UI::Xaml::Media::Geometry>();
}

}  // namespace wxl::impl
