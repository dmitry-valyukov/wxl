#pragma once

#include <winrt/Microsoft.UI.Xaml.h>

#include <optional>
#include <string_view>

#include "../string_param.h"

// What stands behind a window's remembered place -- where it was, how large,
// and whether it was ordinary, maximized or full-screen.
//
// WinUI3 has none of it. AppWindow gives Position and Size, and neither is
// the thing worth remembering: a window closed maximized reports the size of
// the screen, so restoring that size next time hands the reader a window as
// large as their monitor the moment they leave the maximized state. What has
// to be kept is the *restore* rectangle, and only Win32 keeps it --
// WINDOWPLACEMENT.rcNormalPosition, in work-area coordinates, already
// corrected for the taskbar.
//
// Full-screen is not among Win32's states: it is a WinUI presenter, so it is
// carried as a word of its own and applied after the placement.
//
// The value is text on purpose. It is written to whatever the application
// keeps its settings in and handed back unread, so a struct would only be a
// shape for the application to copy around; and read by a person it says
// what it is:
//
//   normal 120 80 1280 860
//   maximized 120 80 1280 860
//   fullscreen 120 80 1280 860
//
// -- the state, then the restore rectangle as left, top, width, height.
// Anything unparseable is ignored rather than throwing: a settings file
// edited by hand, or written by an older version, must not take the window
// down. An empty or missing value leaves the window exactly as it was.
//
// Private: the window arrives as the projection type the wrapper already
// holds, so this header is one only wxl's own sources ever include.

namespace wxl::impl {

// The state a placement string carries, ahead of its restore rectangle. Full
// screen is not a Win32 window state -- the WinUI Window carries it as a
// presenter, the own composited window (CompositionWindow) as a borderless
// cover -- so it is applied by whoever owns the window, not here.
enum class placement_state { normal, maximized, full_screen };

struct placement_geometry {
    placement_state state;
    RECT restore;   // left, top, right, bottom, in work-area coordinates
};

/// Reads "state left top width height". Returns nothing on anything it cannot
/// make sense of -- a settings file edited by hand, or written by an older
/// version, must not take the window down. Shared so both windows read one
/// format one way.
std::optional<placement_geometry> parse_placement(std::wstring_view text);

/// The word a state is written as, for composing the text back.
std::wstring_view name_of(placement_state state);

/// Brings a remembered rectangle to the monitors that exist now: unplugged,
/// resized or rearranged, the old coordinates may put the window where nobody
/// can see it. The window is moved to the nearest display, and centred there
/// if its rectangle misses that display's work area entirely.
RECT fit_placement_to_displays(RECT const& wanted);

/// Puts the window where the text says, if a monitor still agrees.
///
/// A remembered rectangle can mean nothing by the next run: the monitor was
/// unplugged, the resolution changed, the screens swapped places and the
/// coordinates went negative. Restored blindly, the window opens where
/// nobody can see it and the reader decides the application did not start.
/// So the rectangle is checked against the monitors that exist now -- if the
/// one holding most of it is gone, the window moves to wherever the largest
/// remainder is, and failing that to the centre of the primary display.
void set_window_placement(winrt::Microsoft::UI::Xaml::Window const& window,
                          string_param text);

/// The same text, read back off the window as it stands now.
std::wstring get_window_placement(winrt::Microsoft::UI::Xaml::Window const& window);

}  // namespace wxl::impl
