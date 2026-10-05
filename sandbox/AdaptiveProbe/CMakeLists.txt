# Проба D по AdaptiveTrigger: срабатывает ли триггер ширины окна у группы состояний, собранной из кода (без XAML), и на
# каком окне. Чистый C++/WinRT, как sandbox/CppWinRTPanel и по тем же причинам: своя точка входа main, от wxl взяты только
# загрузчик Windows App Runtime и throw_hresult к нему. Приложение консольное, ответы идут в stdout.
add_executable(sandbox.adaptive-probe
    main.cpp
    ${PROJECT_SOURCE_DIR}/wxl.ui/src/impl/bootstrap.cpp
    ${PROJECT_SOURCE_DIR}/wxl.ui/src/impl/hresult.cpp
)

target_include_directories(sandbox.adaptive-probe PRIVATE
    ${WXL_CPPWINRT_PROJECTION_DIR}
    ${PROJECT_SOURCE_DIR}/wxl.ui/src
    ${PROJECT_SOURCE_DIR}/wxl.core/src
    ${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/include
)

target_link_libraries(sandbox.adaptive-probe PRIVATE
    wxl.winappsdk.bootstrap
    Microsoft::CppWinRT
    runtimeobject.lib
)

add_custom_command(TARGET sandbox.adaptive-probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_RUNTIME_DLLS:sandbox.adaptive-probe>" "$<TARGET_FILE_DIR:sandbox.adaptive-probe>"
    COMMAND_EXPAND_LISTS
    COMMENT "Copying the Windows App Runtime bootstrapper beside sandbox.adaptive-probe")

wxl_set_target_folders(sandbox.adaptive-probe)
