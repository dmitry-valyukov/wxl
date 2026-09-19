# Trayed -- a program hosted in a console of ours, managed from the tray.
#
# It links wxl::ui like the other samples, and for the same reason: wxl owns
# the entry point. There is no XAML in it at all -- the console is the window
# and the icon is the interface -- but wWinMain, the Windows App Runtime and
# the STA memory pool all still come from the library, and wxl_launched() is
# still where the application starts.
#
# The GUI subsystem is what makes wxl's own wWinMain the entry point -- and
# what makes the console it allocates its own rather than one it was started
# from.

wxl_add_executable(sample.trayed
    main.cpp
    Settings.h
    Settings.cpp
    settings_window.h
    settings_window.cpp
)

# comctl32 for SetWindowSubclass: the settings window (a wxl/WinUI window) is
# subclassed so its close button hides it rather than ending the application.
target_link_libraries(sample.trayed PRIVATE comctl32)
