# Проба движка композиции ОС (CompositionEngine::TrySetProcessEngine(System),
# Windows App SDK 2.3, Limited Access Feature): даётся ли он процессу, если
# попросить до первого объекта композиции и до XAML. Консольная программа со
# своим main и без wxl.ui: wxl.ui несёт wWinMain, который поднимает XAML раньше,
# чем приложению дают слово. Рантайм поднимается тем же загрузчиком, что и в
# wxl (MddBootstrapInitialize, «release 2»).
add_executable(sandbox.system-engine-probe
    main.cpp
)

target_include_directories(sandbox.system-engine-probe PRIVATE
    ${WXL_CPPWINRT_PROJECTION_DIR}
    ${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/include)
target_link_libraries(sandbox.system-engine-probe PRIVATE
    wxl::core
    Microsoft::CppWinRT
    runtimeobject.lib
    "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/lib/native/x64/Microsoft.WindowsAppRuntime.Bootstrap.lib")
add_custom_command(TARGET sandbox.system-engine-probe POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/runtimes/win-x64/native/Microsoft.WindowsAppRuntime.Bootstrap.dll"
        "$<TARGET_FILE_DIR:sandbox.system-engine-probe>"
    VERBATIM)
wxl_set_target_folders(sandbox.system-engine-probe)
