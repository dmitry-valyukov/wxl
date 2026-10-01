# Проба масштаба у корня острова XAML: доходит ли жест масштабирования до
# ScrollViewer-датчика через список в своём ScrollViewer и гасит ли хук
# потока Ctrl+колесо до XAML. Исследовательский код на проекции cppwinrt
# напрямую, с журналом рядом с исполняемым файлом, -- потому и живёт здесь, а
# не в samples.
wxl_add_executable(sandbox.zoom-gesture-probe
    main.cpp
)

target_include_directories(sandbox.zoom-gesture-probe PRIVATE ${WXL_CPPWINRT_PROJECTION_DIR})
target_link_libraries(sandbox.zoom-gesture-probe PRIVATE Microsoft::CppWinRT runtimeobject.lib user32.lib)
