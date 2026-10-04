#pragma once

// wxl::navigatePage -- Frame.Navigate over a page made on the spot.
//
// Frame.Navigate takes the type of a page, which the framework makes by name
// through the XAML type system; an application of wxl has no XAML pages to
// name. What it has is the Page of the framework itself, an empty page that
// activates like any other, and the content it puts in it: this navigates the
// frame to a new Page -- with all the transitions, the back stack and the
// events that go with a navigation -- and hands the page back, for the
// application to fill.
//
//     auto const page = navigatePage(frame, SlideNavigationTransitionInfo {effect = ...});
//     page.content(Grid {...});
//
// GoBack makes the page again, and empty: whoever goes back fills it again
// (see PagedFrame in the Gallery).

#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Media.Animation.h>

namespace wxl {

/// Navigates `frame` to a new Page, with the default transition of the frame.
Page navigatePage(Frame const& frame);

/// The same, with the transition `info` chosen for this navigation.
Page navigatePage(Frame const& frame, NavigationTransitionInfo const& info);

}  // namespace wxl
