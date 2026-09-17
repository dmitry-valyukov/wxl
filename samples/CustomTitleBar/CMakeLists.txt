# The window of wxl's own with a title bar of its own, written in braces: the
# title bar built right where the window is described, the caption buttons
# drawn by the window at the height of that bar, and the whole island zoomed
# with the keys a browser uses. Like every sample, it links wxl::ui and never
# sees the cppwinrt projection.

add_executable(sample.custom-titlebar WIN32
    main.cpp
    app.manifest
)

wxl_set_target_folders(sample.custom-titlebar)

target_link_libraries(sample.custom-titlebar PRIVATE wxl::ui)

# The Windows App Runtime bootstrapper resolves the framework package at
# startup, so its DLL has to sit beside the executable.
set(_wxl_bootstrap_dll
    "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/runtimes/win-x64/native/Microsoft.WindowsAppRuntime.Bootstrap.dll")

if(EXISTS "${_wxl_bootstrap_dll}")
    add_custom_command(TARGET sample.custom-titlebar POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_wxl_bootstrap_dll}" "$<TARGET_FILE_DIR:sample.custom-titlebar>"
        COMMENT "Copying the Windows App Runtime bootstrapper beside sample.custom-titlebar")
endif()

# CompositionWindow keeps a Win2D texture cache for its backdrop, and the
# window fails to come up without the DLL beside it.
set(_wxl_win2d_dll
    "${WXL_WIN2D_DIR}/runtimes/win-x64/native/Microsoft.Graphics.Canvas.dll")

if(EXISTS "${_wxl_win2d_dll}")
    add_custom_command(TARGET sample.custom-titlebar POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_wxl_win2d_dll}" "$<TARGET_FILE_DIR:sample.custom-titlebar>"
        COMMENT "Copying Microsoft.Graphics.Canvas.dll (Win2D) beside sample.custom-titlebar")
endif()
