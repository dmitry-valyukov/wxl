# Как встать наследником настоящего класса WinUI: не новый тип в системе типов
# WinRT, а COM-агрегация -- IFrameworkElementFactory::CreateInstance(outer,
# &inner, &value), где outer -- наша собственная идентичность. Проба проверяет
# оба правила идентичности (первый и второй переход QueryInterface) на живом
# FrameworkElement, изнутри Application::Start -- вне UI-потока XAML настоящий
# элемент не создаётся вовсе (RPC_E_WRONG_THREAD).
#
# Не wxl_add_executable() и вообще не wxl::ui: у пробы своя точка входа main, а
# wxl::ui уводит вход на свой wWinMain (target_link_options INTERFACE
# /ENTRY:wWinMainCRTStartup) -- тогда линковщик тянет из библиотеки её
# собственный запуск и требует wxl_launched. Приложение консольное, результаты
# идут в stdout.
#
# Вместо библиотеки взяты два файла, которые пробе и нужны: загрузчик Windows
# App Runtime (без него настоящий WinUI недостижим) и throw_hresult к нему.
# Так они компилируются здесь, а не приезжают со всей wxl.
add_executable(sandbox.aggregation
    main.cpp
    ${PROJECT_SOURCE_DIR}/wxl.ui/src/impl/bootstrap.cpp
    ${PROJECT_SOURCE_DIR}/wxl.ui/src/impl/hresult.cpp
)

target_include_directories(sandbox.aggregation PRIVATE
    ${WXL_CPPWINRT_PROJECTION_DIR}
    ${PROJECT_SOURCE_DIR}/wxl.ui/src
    ${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/include
)

target_compile_definitions(sandbox.aggregation PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

target_link_libraries(sandbox.aggregation PRIVATE
    wxl.winappsdk.bootstrap
    Microsoft::CppWinRT
    runtimeobject.lib
)

# Рядом с exe нужна одна DLL -- загрузчика, -- и она единственная, что линкуется
# импортом, поэтому её называет сам CMake. wxl_target_runtime() здесь не к
# месту: он кладёт то, что нужно wxl::ui (Win2D), а проба её не линкует.
add_custom_command(TARGET sandbox.aggregation POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_RUNTIME_DLLS:sandbox.aggregation>" "$<TARGET_FILE_DIR:sandbox.aggregation>"
    COMMAND_EXPAND_LISTS
    COMMENT "Copying the Windows App Runtime bootstrapper beside sandbox.aggregation")

wxl_set_target_folders(sandbox.aggregation)
