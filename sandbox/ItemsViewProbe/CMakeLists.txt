# Воспроизведение ошибки LinedFlowLayout в WinUI (пустой замороженный диапазон при нулевом окне
# просмотра): голая проекция cppwinrt, как sandbox/CppWinRTPanel. Подробности и ключи -- в начале
# main.cpp. Приложение консольное, окно закрывается само; код выхода 0 -- не упало.
add_executable(sandbox.items-view-probe
    main.cpp
    ${PROJECT_SOURCE_DIR}/wxl.ui/src/impl/bootstrap.cpp
    ${PROJECT_SOURCE_DIR}/wxl.ui/src/impl/hresult.cpp
)

target_include_directories(sandbox.items-view-probe PRIVATE
    ${WXL_CPPWINRT_PROJECTION_DIR}
    ${PROJECT_SOURCE_DIR}/wxl.ui/src
    ${PROJECT_SOURCE_DIR}/wxl.core/src
    ${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/include
)

target_link_libraries(sandbox.items-view-probe PRIVATE
    wxl.winappsdk.bootstrap
    Microsoft::CppWinRT
    runtimeobject.lib
)

add_custom_command(TARGET sandbox.items-view-probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_RUNTIME_DLLS:sandbox.items-view-probe>" "$<TARGET_FILE_DIR:sandbox.items-view-probe>"
    COMMAND_EXPAND_LISTS
    COMMENT "Copying the Windows App Runtime bootstrapper beside sandbox.items-view-probe")

wxl_set_target_folders(sandbox.items-view-probe)
