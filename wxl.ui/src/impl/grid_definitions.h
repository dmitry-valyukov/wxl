#pragma once

#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include <string_view>

#include "../string_param.h"

// What stands behind a Grid's rows and columns written as text.
//
// WinUI3 describes them as a collection of objects -- one RowDefinition per
// row, each carrying a GridLength -- which is four lines of markup for what
// every grid in every framework is written as one string: "2*,*,*,auto,120".
// wxl declares the property itself (see the profile's syntheticMembers) and
// this is the body behind it.
//
// The spelling is the familiar one: `*` takes a share of what is left, `N*`
// takes N shares, `auto` is as large as the content needs, a bare number is
// that many pixels. Whitespace around an entry is ignored, and an entry that
// parses as none of these is skipped rather than throwing -- a malformed
// layout string must not take the window down.
//
// Private: the grid arrives as the projection type the wrapper already
// holds, so this header is one only wxl's own sources ever include.

namespace wxl::impl {

void set_row_definitions(winrt::Microsoft::UI::Xaml::Controls::Grid const& grid,
                         string_param spec);
void set_column_definitions(winrt::Microsoft::UI::Xaml::Controls::Grid const& grid,
                            string_param spec);

}  // namespace wxl::impl
