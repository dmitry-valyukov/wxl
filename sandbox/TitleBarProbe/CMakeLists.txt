# Проба своего заголовка окна: окно устроено как wxl::CompositionWindow (HWND
# без поверхности перенаправления, сцена на DesktopAttachedSiteBridge, XAML
# островом DesktopWindowXamlSource), а заголовок ставится теми же публичными
# средствами, которыми его ставит Microsoft.UI.Xaml.Window: AppWindowTitleBar
# и InputNonClientPointerSource. Исследовательский код: пишет журнал рядом с
# исполняемым файлом и видит проекцию cppwinrt напрямую -- приложению на wxl
# ни то, ни другое не нужно, потому проба и живёт здесь, а не в samples.
wxl_add_executable(sandbox.titlebar-probe
    main.cpp
)

target_include_directories(sandbox.titlebar-probe PRIVATE ${WXL_CPPWINRT_PROJECTION_DIR})
target_compile_definitions(sandbox.titlebar-probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX NODRAWTEXT)
target_link_libraries(sandbox.titlebar-probe PRIVATE Microsoft::CppWinRT runtimeobject.lib shell32.lib)
