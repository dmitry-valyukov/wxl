#include "cursor.h"

#include <winrt/Microsoft.UI.Input.h>

namespace wxl::impl {

void set_cursor(winrt::Microsoft::UI::Xaml::UIElement const& element,
                InputSystemCursorShape const shape) {
    // as<>() rather than try_as<>(): an element whose object does not answer
    // for IUIElementProtected is a broken runtime, not a case to handle.
    element.as<winrt::Microsoft::UI::Xaml::IUIElementProtected>().ProtectedCursor(
        winrt::Microsoft::UI::Input::InputSystemCursor::Create(
            static_cast<winrt::Microsoft::UI::Input::InputSystemCursorShape>(shape)));
}

}  // namespace wxl::impl
