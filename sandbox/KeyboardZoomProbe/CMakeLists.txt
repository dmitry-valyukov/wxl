# Проба клавиш масштаба у окна с островом XAML: куда при разном положении
# фокуса попадает Ctrl+«+» и где его можно поймать раньше всех -- в оконной
# процедуре, хуком потока, источником клавиатуры острова или событием XAML.
# Исследовательский код на проекции cppwinrt напрямую, с журналом рядом с
# исполняемым файлом, -- потому и живёт здесь, а не в samples.
wxl_add_executable(sandbox.keyboard-zoom-probe
    main.cpp
)

target_include_directories(sandbox.keyboard-zoom-probe PRIVATE ${WXL_CPPWINRT_PROJECTION_DIR})
target_link_libraries(sandbox.keyboard-zoom-probe PRIVATE Microsoft::CppWinRT runtimeobject.lib user32.lib)
