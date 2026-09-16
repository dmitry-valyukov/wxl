#pragma once

// wxl::makeModalDialog -- a second window turned into a modal dialog of its
// owner, and taken off the taskbar.
//
// WinUI3 gives a Window no modal mode and no owner: a ContentDialog is modal but
// is not a window, and a Flyout the same. So a dialog that must be a real second
// window -- its own HWND, its own XamlRoot -- is made modal the way Win32 has
// always done it, wrapped here so an application need not: the owner is set (the
// one thing that has no WinUI API, like the HWND itself -- see WindowHandle.h),
// and then WinUI's own OverlappedPresenter.IsModal disables that owner while the
// dialog is open. Off the taskbar follows from being owned, and is asked for
// outright besides (IsShownInSwitchers), so neither a taskbar button nor an
// Alt+Tab entry appears.
//
// Minimize and maximize are turned off with it: a modal dialog is a fixed thing
// the user answers, not a window they park or grow.

#include "core.h"

namespace wxl {

class Window;

/// Makes `dialog` modal to `owner` -- the owner is disabled until the dialog
/// closes -- and keeps it off the taskbar and out of Alt+Tab. Both windows must
/// already be created (their handles exist); call it before activating `dialog`.
void makeModalDialog(Window const& dialog, Window const& owner);

}  // namespace wxl
