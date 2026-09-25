# Своя панель на шаблоне cppwinrt PanelT<D>: наследник настоящего Panel через
# агрегацию, которую делает сам cppwinrt. Проба отвечает, создаётся ли такая
# панель без регистрации своего класса и зовёт ли XAML её MeasureOverride и
# ArrangeOverride.
#
# Устроена как sandbox/Aggregation и по тем же причинам: своя точка входа main,
# а не wxl_add_executable() -- wxl::ui уводит вход на свой wWinMain; от wxl
# взяты только загрузчик Windows App Runtime и throw_hresult к нему.
# Приложение консольное, ответы идут в stdout.
add_executable(sandbox.cppwinrt-panel
    main.cpp
    ${PROJECT_SOURCE_DIR}/wxl.ui/src/impl/bootstrap.cpp
    ${PROJECT_SOURCE_DIR}/wxl.ui/src/impl/hresult.cpp
)

target_include_directories(sandbox.cppwinrt-panel PRIVATE
    ${WXL_CPPWINRT_PROJECTION_DIR}
    ${PROJECT_SOURCE_DIR}/wxl.ui/src
    ${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/include
)

target_compile_definitions(sandbox.cppwinrt-panel PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

target_link_libraries(sandbox.cppwinrt-panel PRIVATE
    wxl.winappsdk.bootstrap
    Microsoft::CppWinRT
    runtimeobject.lib
)

# Рядом с exe нужна одна DLL -- загрузчика; как в sandbox/Aggregation.
add_custom_command(TARGET sandbox.cppwinrt-panel POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_RUNTIME_DLLS:sandbox.cppwinrt-panel>" "$<TARGET_FILE_DIR:sandbox.cppwinrt-panel>"
    COMMAND_EXPAND_LISTS
    COMMENT "Copying the Windows App Runtime bootstrapper beside sandbox.cppwinrt-panel")

wxl_set_target_folders(sandbox.cppwinrt-panel)
