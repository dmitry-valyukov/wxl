# Витрина эффектов: по эффекту на .cpp, общая раскладка страницы в
# Showcase.cpp, окно со списком слева в main.cpp. Как любое приложение на wxl,
# линкует только wxl::ui; WIN32 делает точкой входа wWinMain самой wxl.

add_executable(sample.effects WIN32
    Pages.h
    pch.h
    main.cpp
    Showcase.cpp
    HaloPage.cpp
    MagnifyPage.cpp
    app.manifest
)

wxl_set_target_folders(sample.effects)

target_link_libraries(sample.effects PRIVATE wxl::ui)

# Шрифты и картинки образца -- его собственные, из его Assets, рядом с exe.
wxl_copy_assets(sample.effects FROM "${CMAKE_CURRENT_SOURCE_DIR}/Assets")

# Загрузчик Windows App Runtime ищет пакет платформы при старте и должен лежать
# рядом с исполняемым файлом.
set(_wxl_bootstrap_dll
    "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/runtimes/win-x64/native/Microsoft.WindowsAppRuntime.Bootstrap.dll")

if(EXISTS "${_wxl_bootstrap_dll}")
    add_custom_command(TARGET sample.effects POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_wxl_bootstrap_dll}" "$<TARGET_FILE_DIR:sample.effects>"
        COMMENT "Копирование загрузчика Windows App Runtime рядом с sample.effects")
else()
    message(WARNING
        "No Microsoft.WindowsAppRuntime.Bootstrap.dll at ${_wxl_bootstrap_dll}; "
        "sample.effects will fail to start until it is beside the executable.")
endif()
