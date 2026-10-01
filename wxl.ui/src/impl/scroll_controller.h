#pragma once

#include <winrt/Microsoft.UI.Xaml.Controls.h>

// What stands behind `verticalScrollController = annotatedBar` on a ScrollView.
//
// A ScrollView scrolls through a ScrollPresenter inside it, and the presenter
// takes a scroll controller for each direction: the object that drives the
// offset from outside and is told of every change. An AnnotatedScrollBar
// *is* one, through its ScrollController property -- a property of the bar
// that hands out an interface no wrapper stands for. wxl writes the link as a
// property of the view, holding the bar, and makes it here.
//
// Private: the view arrives as the projection type the wrapper already holds,
// so this header is one only wxl's own sources ever include.

namespace wxl {
class AnnotatedScrollBar;
}  // namespace wxl

namespace wxl::impl {

void set_vertical_scroll_controller(winrt::Microsoft::UI::Xaml::Controls::ScrollView const& view,
                                    AnnotatedScrollBar const& bar);

void set_horizontal_scroll_controller(winrt::Microsoft::UI::Xaml::Controls::ScrollView const& view,
                                      AnnotatedScrollBar const& bar);

}  // namespace wxl::impl
