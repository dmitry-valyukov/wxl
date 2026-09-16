# Trayed -- a program hosted in a console of ours, managed from the tray.
#
# It links wxl::ui like the other samples, and for the same reason: wxl owns
# the entry point. There is no XAML in it at all -- the console is the window
# and the icon is the interface -- but wWinMain, the Windows App Runtime and
# the STA memory pool all still come from the library, and wxl_launched() is
# still where the application starts.
#
# WIN32 makes it a GUI subsystem executable, which is what makes wxl's own
# wWinMain the entry point -- and what makes the console it allocates its own
# rather than one it was started from.

add_executable(sample.trayed WIN32
    main.cpp
    Settings.h
    Settings.cpp
    settings_window.h
    settings_window.cpp
    app.manifest
)

wxl_set_target_folders(sample.trayed)

# comctl32 for SetWindowSubclass: the settings window (a wxl/WinUI window) is
# subclassed so its close button hides it rather than ending the application.
target_link_libraries(sample.trayed PRIVATE wxl::ui comctl32)

# The Windows App Runtime bootstrapper resolves the framework package at
# startup, so its DLL has to sit beside the executable. wxl links the import
# library itself; this is the other half of "wxl owns its own runtime
# dependencies" -- a consuming application still writes none of it.
set(_wxl_bootstrap_dll
    "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/runtimes/win-x64/native/Microsoft.WindowsAppRuntime.Bootstrap.dll")

if(EXISTS "${_wxl_bootstrap_dll}")
    add_custom_command(TARGET sample.trayed POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_wxl_bootstrap_dll}" "$<TARGET_FILE_DIR:sample.trayed>"
        COMMENT "Copying the Windows App Runtime bootstrapper beside sample.trayed")
else()
    message(WARNING
        "No Microsoft.WindowsAppRuntime.Bootstrap.dll at ${_wxl_bootstrap_dll}; "
        "sample.trayed will fail to start until it is beside the executable.")
endif()
