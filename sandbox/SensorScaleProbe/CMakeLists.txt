# Проба: держится ли кнопка у правого края острова XAML при смене масштаба
# острова (OverrideScale моста, как ZoomEffect), если над корнем стоит
# ScrollViewer-датчик жеста, как в CompositionWindow, -- и без него.
# Исследовательский код на проекции cppwinrt напрямую, с журналом рядом с
# исполняемым файлом.
wxl_add_executable(sandbox.sensor-scale-probe
    main.cpp
)

target_include_directories(sandbox.sensor-scale-probe PRIVATE ${WXL_CPPWINRT_PROJECTION_DIR})
target_link_libraries(sandbox.sensor-scale-probe PRIVATE Microsoft::CppWinRT runtimeobject.lib user32.lib)
