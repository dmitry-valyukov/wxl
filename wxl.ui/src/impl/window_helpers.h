#pragma once

// What stands behind the window properties WinUI 3 has no property for, and the parts of a
// window's frame and place only Win32 and DWM keep.
//
// Private: the window arrives as the projection type the wrapper already holds, so this header is
// one only wxl's own sources ever include.

#include "platform.h"

#include <winrt/Microsoft.UI.Xaml.h>

#include <cstdint>
#include <optional>
#include <string_view>

#include "../SizeToContent.h"
#include "../core.h"
#include "../hstring_param.h"
#include <wxl/Windows.Graphics.Structs.h>

namespace wxl {
class MicaController;
class DesktopAcrylicController;
}  // namespace wxl

namespace wxl::impl {

/// A window this code made and owns: destroyed when its owner goes. One the system destroyed
/// first -- closed by the user, its last message (WM_NCDESTROY) come -- is let go of by release(),
/// and nothing is destroyed twice. Made from a window that exists: whoever creates it throws on a
/// failure first.
class owned_window : core::noncopyable {
public:
    explicit owned_window(HWND window) noexcept : window_(window) {}

    ~owned_window() {
        if (window_) ::DestroyWindow(window_);
    }

    /// The window, or null once it is gone.
    HWND get() const noexcept { return window_; }

    /// The window is gone without this owner: nothing is left to destroy.
    void release() noexcept { window_ = nullptr; }

private:
    HWND window_;
};

/// The smallest the user may make the window, in the raw pixels AppWindow itself works in. It is
/// kept on the window's overlapped presenter, three hops from the Window, so wxl declares the
/// property itself (the profile's syntheticMembers). A zero side is left unbounded, which is what
/// the runtime reads an unset preference as.
void set_minimum_size(winrt::Microsoft::UI::Xaml::Window const& window, SizeInt32 const& size);

/// Window's `sizeToContent` (the profile's syntheticMembers): the sides `mode` names follow the
/// content, once -- when the window first activates its content is read, and when that content has
/// been laid out the window is resized to what it wanted. Read at activation rather than here,
/// because the content may be written after the tag in the window's braces. One side keeps the
/// other as the window has it. A window with no content to size to is a mistake, and stops.
void set_size_to_content(winrt::Microsoft::UI::Xaml::Window const& window, SizeToContent mode);

// A window's remembered place: the state and the *restore* rectangle -- what Win32 keeps in
// WINDOWPLACEMENT.rcNormalPosition, in work-area coordinates. AppWindow's Position and Size are not
// worth remembering: a window closed maximized reports the size of the screen. The value is text,
// readable by a person:
//
//   normal 120 80 1280 860
//   maximized 120 80 1280 860
//   fullscreen 120 80 1280 860
//
// -- the state, then left, top, width, height. Anything unparseable is ignored rather than
// thrown: a settings file edited by hand, or written by an older version, must not take the window
// down. An empty value leaves the window as it was. Defined in WindowNative.cpp.

/// The state a placement string carries. Full screen is not a Win32 window state -- the WinUI
/// Window carries it as a presenter, CompositionWindow as a borderless cover -- so it is applied
/// by whoever owns the window.
enum class placement_state { normal, maximized, full_screen };

struct placement_geometry {
    placement_state state;
    RECT restore;   // left, top, right, bottom, in work-area coordinates
};

/// Reads "state left top width height"; nothing on anything it cannot make sense of. Shared so
/// both windows read one format one way.
std::optional<placement_geometry> parse_placement(std::u16string_view text);

/// The word a state is written as, for composing the text back.
std::u16string_view name_of(placement_state state);

/// Brings a remembered rectangle to the monitors that exist now: the window is moved to the
/// nearest display, and centred there if its rectangle misses that display's work area entirely.
RECT fit_placement_to_displays(RECT const& wanted);

/// The setter of Window's placement property (the profile's syntheticMembers). A friend of
/// WindowNative, which keeps the rectangle the window returns to; reading it back is
/// WindowNative::placement().
struct window_placement_setter {
    /// Puts the window where the text says, fitted to the monitors that exist now: restored
    /// blindly, a window whose monitor was unplugged opens where nobody can see it.
    static void set(winrt::Microsoft::UI::Xaml::Window const& window, hstring_param const& text);
};

/// DWMWA_USE_IMMERSIVE_DARK_MODE: the frame in the dark theme, or back in the light one. With
/// frame_caption, defined in window_frame.cpp of its own: dwmapi.h drags in uxtheme.h, which wants
/// the GDI types NOGDI keeps out of the rest of the library, and NOGDI is lifted there alone.
void frame_dark(HWND__* hwnd, bool on) noexcept;

/// DWMWA_CAPTION_COLOR and DWMWA_TEXT_COLOR, as COLORREFs (0x00BBGGRR). Windows 11; where DWM
/// does not know them the call has no effect and the frame stays the system's.
void frame_caption(HWND__* hwnd, uint32_t face, uint32_t ink) noexcept;

/// `isDragRegion` on an element inside a TitleBar. The TitleBar decides by itself which of its
/// content drags the window -- an interactive control is clickable, anything else drags; true
/// always drags, false is always clickable, and an empty value gives the choice back.
void set_drag_region(winrt::Microsoft::UI::Xaml::UIElement const& element,
                     core::nullable<bool> const& value);

// `micaController = ...`, `desktopAcrylicController = ...` and `noSystemBackdrop = true` on a
// Window. A system backdrop controller is told which window to draw under by the window's
// ICompositionSupportsSystemBackdrop, an interface no wrapper declares and the window answers to
// QueryInterface alone; the controller then needs its configuration and, when the window closes,
// Close.

void clear_system_backdrop(winrt::Microsoft::UI::Xaml::Window const& window, bool clear);

void set_mica_controller(winrt::Microsoft::UI::Xaml::Window const& window,
                         MicaController const& controller);

void set_desktop_acrylic_controller(winrt::Microsoft::UI::Xaml::Window const& window,
                                    DesktopAcrylicController const& controller);

}  // namespace wxl::impl
