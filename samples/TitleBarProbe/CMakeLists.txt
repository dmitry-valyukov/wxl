# Проба своего заголовка окна: окно устроено как wxl::CompositionWindow (HWND
# без поверхности перенаправления, сцена на DesktopAttachedSiteBridge, XAML
# островом DesktopWindowXamlSource), а заголовок ставится теми же публичными
# средствами, которыми его ставит Microsoft.UI.Xaml.Window: AppWindowTitleBar
# и InputNonClientPointerSource. Исследовательский код: пишет журнал рядом с
# исполняемым файлом и потому, в отличие от других образцов, видит проекцию
# cppwinrt напрямую.
add_executable(sample.titlebar-probe WIN32
    main.cpp
    app.manifest
)

wxl_set_target_folders(sample.titlebar-probe)

target_include_directories(sample.titlebar-probe PRIVATE ${WXL_CPPWINRT_PROJECTION_DIR})
target_compile_definitions(sample.titlebar-probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX NODRAWTEXT)
target_link_libraries(sample.titlebar-probe PRIVATE wxl::ui Microsoft::CppWinRT runtimeobject.lib shell32.lib)

set(_wxl_bootstrap_dll
    "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/runtimes/win-x64/native/Microsoft.WindowsAppRuntime.Bootstrap.dll")

if(EXISTS "${_wxl_bootstrap_dll}")
    add_custom_command(TARGET sample.titlebar-probe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_wxl_bootstrap_dll}" "$<TARGET_FILE_DIR:sample.titlebar-probe>"
        COMMENT "Копирование загрузчика Windows App Runtime рядом с sample.titlebar-probe")
endif()
