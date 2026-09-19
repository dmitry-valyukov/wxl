# Проба своего заголовка окна: окно устроено как wxl::CompositionWindow (HWND
# без поверхности перенаправления, сцена на DesktopAttachedSiteBridge, XAML
# островом DesktopWindowXamlSource), а заголовок ставится теми же публичными
# средствами, которыми его ставит Microsoft.UI.Xaml.Window: AppWindowTitleBar
# и InputNonClientPointerSource. Исследовательский код: пишет журнал рядом с
# исполняемым файлом и потому, в отличие от других образцов, видит проекцию
# cppwinrt напрямую.
wxl_add_executable(sample.titlebar-probe
    main.cpp
    app.manifest
)

target_include_directories(sample.titlebar-probe PRIVATE ${WXL_CPPWINRT_PROJECTION_DIR})
target_compile_definitions(sample.titlebar-probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX NODRAWTEXT)
target_link_libraries(sample.titlebar-probe PRIVATE Microsoft::CppWinRT runtimeobject.lib shell32.lib)
