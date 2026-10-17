#pragma once

// wxl::showDialog -- a ContentDialog put on screen, and the wait for its answer left running.
//
// ContentDialog has one door, ShowAsync, and its wrapper hands it out as a task to await
// (dialog.showAsync(stop)). Many dialogs are not awaited, though: a notice, an "about" -- the
// caller shows one and goes on, and the answer, where there is one, comes the way everything
// else in wxl does -- through the dialog's own events (PrimaryButtonClick, CloseButtonClick,
// Closed and their neighbours), which are projected already. This is that call: it returns at
// once, and the wait is the library's.
//
// The wait is a detached coroutine, the process's like every other: the exit waits for it, so a
// dialog left up keeps the process waiting. Hence the token, and no form without one: it is how
// the owner closes the dialog -- a token of the application's common source, cancelled on the
// way out while the window's queue still turns (from the main window's Closed at the latest:
// the wait at exit comes before Teardown), or of a source made under it, for a dialog that has
// to go sooner. Cancelled, the token posts a turn to the thread's queue, and that turn asks the
// dialog to close -- it closes without raising Closing, which could refuse, and raises Closed
// once it is done -- and ends the wait. Under a token cancelled already the dialog is not shown.
// A token with no source is one nobody can cancel, and the call stops on it. impl/show_dialog.h
// has the mechanism.
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
// refusal is an exception, thrown from this call -- as is the refusal of a
// dialog with no XamlRoot to show over; nothing is left running then. Whoever
// opens dialogs from a handler that can fire twice catches it or keeps track
// itself -- there is nothing here to keep it for them.

#include "Operation.h"

namespace wxl {

class ContentDialog;
class UIElement;
class Window;

/// Shows `dialog` over `host` and returns at once -- the dialog stays up
/// until the reader answers it, and what they answered arrives on the
/// dialog's events, or until `stop` is cancelled, which closes it.
/// \throw the platform's refusal, when the dialog cannot be shown.
void showDialog(ContentDialog const& dialog, Window const& host, async::cancellation_token stop);

/// The same, over the island `host` is in.
void showDialog(ContentDialog const& dialog, UIElement const& host, async::cancellation_token stop);

}  // namespace wxl
