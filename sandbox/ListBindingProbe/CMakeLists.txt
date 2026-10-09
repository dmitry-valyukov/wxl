# Проба к привязке списка к наблюдаемой коллекции: что списочные контролы WinUI делают с источником, с отданными
# элементами и с состоянием шаблона wxl. Вопросы и как читать ответ -- README.md рядом.
#
# Две программы. sandbox.list-binding-probe -- приложение на wxl (wxl_add_executable): точка входа wxl и ListView,
# построенный самим wxl (вопрос (г)); остальное в нём написано на проекции cppwinrt напрямую (probes.cpp), как у
# sandbox/WindowlessIslandProbe. sandbox.list-binding-windowless -- вопрос (е): XAML без Application::Start, поэтому
# своя точка входа main и без wxl.ui, как sandbox/CppWinRTPanel; первая запускает её сама и вставляет её ответ в свой.
wxl_add_executable(sandbox.list-binding-probe
    main.cpp
    probes.cpp
)

target_include_directories(sandbox.list-binding-probe PRIVATE ${WXL_CPPWINRT_PROJECTION_DIR})
target_link_libraries(sandbox.list-binding-probe PRIVATE Microsoft::CppWinRT runtimeobject.lib user32.lib)

add_executable(sandbox.list-binding-windowless
    windowless.cpp
    ${PROJECT_SOURCE_DIR}/wxl.ui/src/impl/bootstrap.cpp
    ${PROJECT_SOURCE_DIR}/wxl.ui/src/impl/hresult.cpp
)

target_include_directories(sandbox.list-binding-windowless PRIVATE
    ${WXL_CPPWINRT_PROJECTION_DIR}
    ${PROJECT_SOURCE_DIR}/wxl.ui/src
    ${PROJECT_SOURCE_DIR}/wxl.core/src
    ${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/include
)

target_link_libraries(sandbox.list-binding-windowless PRIVATE
    wxl.winappsdk.bootstrap
    Microsoft::CppWinRT
    runtimeobject.lib
    user32.lib
)

# Рядом с exe нужна одна DLL -- загрузчика; как в sandbox/CppWinRTPanel.
add_custom_command(TARGET sandbox.list-binding-windowless POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_RUNTIME_DLLS:sandbox.list-binding-windowless>" "$<TARGET_FILE_DIR:sandbox.list-binding-windowless>"
    COMMAND_EXPAND_LISTS
    COMMENT "Copying the Windows App Runtime bootstrapper beside sandbox.list-binding-windowless")

wxl_set_target_folders(sandbox.list-binding-windowless)

# Проба ищет вторую программу рядом с собой: собирается одна цель -- собираются обе.
add_dependencies(sandbox.list-binding-probe sandbox.list-binding-windowless)
