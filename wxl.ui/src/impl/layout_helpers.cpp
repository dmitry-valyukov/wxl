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

// Imports last, after every plain header.
import wxl.core;

namespace wxl::impl {

namespace {

using winrt::Microsoft::UI::Xaml::GridLength;
using winrt::Microsoft::UI::Xaml::GridUnitType;

// "*", "2*", "auto", "120" -- the four forms, in the order they are told
// apart. A star with no number in front of it is one share.
std::optional<GridLength> parse_length(std::u16string_view entry) {
    if (entry.empty()) {
        return std::nullopt;
    }
    if (entry == u"auto" || entry == u"Auto") {
        return GridLength{1.0, GridUnitType::Auto};
    }

    bool const star = entry.back() == u'*';
    if (star) {
        entry.remove_suffix(1);
        if (entry.empty()) {
            return GridLength{1.0, GridUnitType::Star};
        }
    }

    // The whole entry has to be the number, read straight out of the view: no
    // copy to get a terminator, and no locale to decide what a decimal point
    // is. std::wcstod read the process's LC_NUMERIC, so "1.5*" would have
    // become 1 wherever that had been set to a comma.
    // A grid is described on the GUI thread, so the narrowing buffer -- should
    // an entry ever outgrow its stack half -- comes from that thread's pool.
    double value = 0;
    if (!wxl::core::try_parse<wxl::core::sta_allocator>(entry, value)) {
        return std::nullopt;
    }
    return GridLength{value, star ? GridUnitType::Star : GridUnitType::Pixel};
}

// Each comma-separated entry in turn, as a GridLength.
template <typename Add>
void parse_each(std::u16string_view spec, Add add) {
    for (std::u16string_view const entry : wxl::core::split(spec, u',')) {
        if (auto const length = parse_length(wxl::core::trim(entry))) {
            add(*length);
        }
    }
}

}  // namespace

void set_row_definitions(winrt::Microsoft::UI::Xaml::Controls::Grid const& grid,
                         hstring_param const& text) {
    std::u16string_view const spec = text.text().view();
    auto const rows = grid.RowDefinitions();
    rows.Clear();
    parse_each(spec, [&](GridLength const& length) {
        winrt::Microsoft::UI::Xaml::Controls::RowDefinition row;
        row.Height(length);
        rows.Append(row);
    });
}

void set_column_definitions(winrt::Microsoft::UI::Xaml::Controls::Grid const& grid,
                            hstring_param const& text) {
    std::u16string_view const spec = text.text().view();
    auto const columns = grid.ColumnDefinitions();
    columns.Clear();
    parse_each(spec, [&](GridLength const& length) {
        winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition column;
        column.Width(length);
        columns.Append(column);
    });
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
