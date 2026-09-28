# Проба XAML без дочернего окна: окно устроено как wxl::CompositionWindow
# (HWND с WS_EX_NOREDIRECTIONBITMAP, сцена -- ContentIsland на
# DesktopAttachedSiteBridge, задник в размер из WM_SIZE), а XAML вставлен
# либо, как сейчас, DesktopWindowXamlSource (дочернее окно, свой композитор у
# сцены), либо XamlIsland через ChildSiteLink в сцену на композиторе XAML --
# один композитор на окно. Вопросы: работает ли второе вообще, доходит ли ввод,
# успевает ли задник при быстрой растяжке. Исследовательский код на проекции
# cppwinrt напрямую, с журналом рядом с исполняемым файлом.
wxl_add_executable(sandbox.windowless-island-probe
    main.cpp
)

target_include_directories(sandbox.windowless-island-probe PRIVATE ${WXL_CPPWINRT_PROJECTION_DIR})
target_compile_definitions(sandbox.windowless-island-probe PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX NODRAWTEXT)
target_link_libraries(sandbox.windowless-island-probe PRIVATE Microsoft::CppWinRT runtimeobject.lib user32.lib gdi32.lib)
