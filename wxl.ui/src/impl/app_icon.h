#pragma once

// The application's icon, put on the windows that came up without one.
//
// Windows takes a program's icon from its resources, but a window does not:
// the title bar, the taskbar button and Alt+Tab show the icon of the window,
// and a window that was never given one shows the system's default. WinUI does
// not look at the executable's resources either, so every application would
// have to call SetIcon for itself -- with a path to a file shipped beside the
// executable, which is one more thing to copy and to keep in step.
//
// wxl says it instead, once, right after the application has built its windows:
// resource id 1 -- the one the shell reads and the one wxl::TrayIcon looks up --
// goes onto every top-level window of the thread that carries no icon yet. A
// window whose application set an icon of its own is left alone, and so is an
// executable built without the resource.
//
// Windows of wxl's own class need none of this: CompositionWindow registers the
// icon with the class, so one it creates later -- after this has run -- has it
// from the first frame. The sweep is for the windows wxl does not create, the
// WinUI ones.
//
// Declared without any Windows type, so including it costs a caller nothing.

namespace wxl::impl {

void apply_application_icon() noexcept;

}  // namespace wxl::impl
