# The sample application -- an application as one written against wxl looks:
# it links wxl::ui and nothing else, and it never sees the cppwinrt
# projection, because reaching wxl's declarative surface must not cost a
# consumer the projection's parse time.
#
# WIN32 makes it a GUI subsystem executable, which is what makes wxl's own
# wWinMain the entry point.

add_executable(sample.hello WIN32
    main.cpp
    app.manifest
)

wxl_set_target_folders(sample.hello)

wxl_target_icon(sample.hello Assets/WinUl-logo.ico)

target_link_libraries(sample.hello PRIVATE wxl::ui)

# An image path carrying no scheme is resolved next to the executable, so
# the assets have to be there -- copied on every build, since editing one is
# not a reason to relink.
add_custom_command(TARGET sample.hello POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Assets" "$<TARGET_FILE_DIR:sample.hello>/Assets"
    COMMENT "Copying the sample's assets beside sample.hello")

# The Windows App Runtime bootstrapper resolves the framework package at
# startup, so its DLL has to sit beside the executable. wxl links the import
# library itself; this is the other half of "wxl owns its own runtime
# dependencies" -- a consuming application still writes none of it.
set(_wxl_bootstrap_dll
    "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/runtimes/win-x64/native/Microsoft.WindowsAppRuntime.Bootstrap.dll")

if(EXISTS "${_wxl_bootstrap_dll}")
    add_custom_command(TARGET sample.hello POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_wxl_bootstrap_dll}" "$<TARGET_FILE_DIR:sample.hello>"
        COMMENT "Copying the Windows App Runtime bootstrapper beside sample.hello")
else()
    message(WARNING
        "No Microsoft.WindowsAppRuntime.Bootstrap.dll at ${_wxl_bootstrap_dll}; "
        "sample.hello will fail to start until it is beside the executable.")
endif()
