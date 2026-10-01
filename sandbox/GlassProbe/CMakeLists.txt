# Проба стекла (backdrop-кисть с размытием, как у GlassEffect) в острове XAML:
# видит ли кисть в острове сцену окна под ним -- при DesktopWindowXamlSource
# (как сейчас) и при XamlIsland через ChildSiteLink в сцене на одном или на
# двух композиторах. Исследовательский код на проекции cppwinrt напрямую, с
# журналом и снимками рядом с исполняемым файлом.
wxl_add_executable(sandbox.glass-probe
    main.cpp
)

target_include_directories(sandbox.glass-probe PRIVATE ${WXL_CPPWINRT_PROJECTION_DIR})
target_link_libraries(sandbox.glass-probe PRIVATE Microsoft::CppWinRT runtimeobject.lib user32.lib gdi32.lib)
