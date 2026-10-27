# Проба движка композиции ОС (CompositionEngine::TrySetProcessEngine(System),
# Windows App SDK 2.3, Limited Access Feature): даётся ли он процессу, если
# попросить до первого объекта композиции и до XAML. Консольная программа со
# своим main и без wxl.ui: wxl.ui несёт wWinMain, который поднимает XAML раньше,
# чем приложению дают слово. Windows App Runtime поднимает загрузчик самой wxl
# (impl/bootstrap.cpp); тег версии выпуска загрузчику передаёт цель
# wxl.winappsdk.bootstrap, так же как в wxl.ui.
add_executable(sandbox.system-engine-probe
    main.cpp
    ${PROJECT_SOURCE_DIR}/wxl.ui/src/impl/bootstrap.cpp
    ${PROJECT_SOURCE_DIR}/wxl.ui/src/impl/hresult.cpp
)

target_include_directories(sandbox.system-engine-probe PRIVATE
    ${WXL_CPPWINRT_PROJECTION_DIR}
    ${PROJECT_SOURCE_DIR}/wxl.ui/src
    ${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/include)
target_link_libraries(sandbox.system-engine-probe PRIVATE
    wxl::core
    wxl.winappsdk.bootstrap
    Microsoft::CppWinRT
    runtimeobject.lib)
add_custom_command(TARGET sandbox.system-engine-probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_RUNTIME_DLLS:sandbox.system-engine-probe>" "$<TARGET_FILE_DIR:sandbox.system-engine-probe>"
    COMMAND_EXPAND_LISTS
    COMMENT "Copying the Windows App Runtime bootstrapper beside sandbox.system-engine-probe")
# Манифест исполняемых файлов wxl, как у всякой программы дерева на wxl.
wxl_target_manifest(sandbox.system-engine-probe)
wxl_set_target_folders(sandbox.system-engine-probe)
