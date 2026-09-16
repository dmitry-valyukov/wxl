// The projection comes first, and with it every standard header it needs:
// wxl's own headers carry the wxl.core import, and a standard header included
// after that import is one the compiler has already seen through the std
// module.
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include "grid_definitions.h"

// Imports last, after every plain header.
import wxl.core;

namespace wxl::impl {
namespace {

using winrt::Microsoft::UI::Xaml::GridLength;
using winrt::Microsoft::UI::Xaml::GridUnitType;

// "*", "2*", "auto", "120" -- the four forms, in the order they are told
// apart. A star with no number in front of it is one share.
std::optional<GridLength> parse_length(std::wstring_view entry) {
    if (entry.empty()) {
        return std::nullopt;
    }
    if (entry == L"auto" || entry == L"Auto") {
        return GridLength{1.0, GridUnitType::Auto};
    }

    bool const star = entry.back() == L'*';
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
void parse_each(std::wstring_view spec, Add add) {
    for (std::wstring_view const entry : wxl::core::split(spec, L',')) {
        if (auto const length = parse_length(wxl::core::trim(entry))) {
            add(*length);
        }
    }
}

}  // namespace

void set_row_definitions(winrt::Microsoft::UI::Xaml::Controls::Grid const& grid,
                         string_param text) {
    std::wstring_view const spec = text.wide();
    auto const rows = grid.RowDefinitions();
    rows.Clear();
    parse_each(spec, [&](GridLength const& length) {
        winrt::Microsoft::UI::Xaml::Controls::RowDefinition row;
        row.Height(length);
        rows.Append(row);
    });
}

void set_column_definitions(winrt::Microsoft::UI::Xaml::Controls::Grid const& grid,
                            string_param text) {
    std::wstring_view const spec = text.wide();
    auto const columns = grid.ColumnDefinitions();
    columns.Clear();
    parse_each(spec, [&](GridLength const& length) {
        winrt::Microsoft::UI::Xaml::Controls::ColumnDefinition column;
        column.Width(length);
        columns.Append(column);
    });
}

}  // namespace wxl::impl
