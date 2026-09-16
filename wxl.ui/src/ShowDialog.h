#pragma once

// wxl::showDialog -- a ContentDialog put on screen.
//
// ContentDialog.ShowAsync is the one door the control has, and it returns an
// IAsyncOperation, which is not a wxl type and never will be: the projection
// hands an application wrappers, not awaitables. So the awaiting is done
// here, once, and the answer comes back the way everything else in wxl does
// -- through the dialog's own events (PrimaryButtonClick, CloseButtonClick
// and their neighbours), which are projected already.
//
// The XamlRoot is set on the way in. A ContentDialog is not part of the
// visual tree, and in a desktop application there can be more than one
// island; without a XamlRoot the framework does not know which of them the
// dialog belongs over, and ShowAsync fails outright. Taking the window -- or
// any element already on screen -- instead of the root makes that impossible
// to forget.
//
// The element overload is the one a CompositionWindow uses: its XAML lives in
// an island the window hands out as content(), and there is no wxl::Window to
// name. It is also what a caller reaches for when the dialog belongs over one
// particular island out of several.
//
// One dialog at a time: XAML refuses a second while the first is up, and the
// refusal is an exception. Whoever opens dialogs from a handler that can fire
// twice has to keep track of that itself -- there is nothing here to keep it
// for them.

#include "core.h"

namespace wxl {

class ContentDialog;
class UIElement;
class Window;

/// Shows `dialog` over `host` and returns at once -- the dialog stays up
/// until the reader answers it, and what they answered arrives on the
/// dialog's events.
void showDialog(ContentDialog const& dialog, Window const& host);

/// The same, over the island `host` is in.
void showDialog(ContentDialog const& dialog, UIElement const& host);

}  // namespace wxl
