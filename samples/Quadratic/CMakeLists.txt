# Квадратное уравнение: модель на observable и интерфейс одним выражением в окне
# CompositionWindow с фоном плиткой, всё в main.cpp. Как любое приложение на wxl,
# линкует только wxl::ui; WIN32 делает точкой входа wWinMain самой wxl.

add_executable(sample.quadratic WIN32
    main.cpp
    app.manifest
)

wxl_set_target_folders(sample.quadratic)

target_link_libraries(sample.quadratic PRIVATE wxl::ui)

# Загрузчик Windows App Runtime ищет пакет платформы при старте и должен лежать
# рядом с исполняемым файлом.
set(_wxl_bootstrap_dll
    "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/runtimes/win-x64/native/Microsoft.WindowsAppRuntime.Bootstrap.dll")

if(EXISTS "${_wxl_bootstrap_dll}")
    add_custom_command(TARGET sample.quadratic POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_wxl_bootstrap_dll}" "$<TARGET_FILE_DIR:sample.quadratic>"
        COMMENT "Копирование загрузчика Windows App Runtime рядом с sample.quadratic")
else()
    message(WARNING
        "No Microsoft.WindowsAppRuntime.Bootstrap.dll at ${_wxl_bootstrap_dll}; "
        "sample.quadratic will fail to start until it is beside the executable.")
endif()

# Картинка фона ищется рядом с исполняемым файлом -- папка Assets копируется при
# каждой сборке: её правка -- не повод перелинковывать.
add_custom_command(TARGET sample.quadratic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/Assets" "$<TARGET_FILE_DIR:sample.quadratic>/Assets"
    COMMENT "Копирование ресурсов примера рядом с sample.quadratic")

# CompositionWindow держит кэш текстур Win2D, и без его DLL рядом окно не
# поднимается.
set(_wxl_win2d_dll
    "${WXL_WIN2D_DIR}/runtimes/win-x64/native/Microsoft.Graphics.Canvas.dll")

if(EXISTS "${_wxl_win2d_dll}")
    add_custom_command(TARGET sample.quadratic POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_wxl_win2d_dll}" "$<TARGET_FILE_DIR:sample.quadratic>"
        COMMENT "Копирование Microsoft.Graphics.Canvas.dll (Win2D) рядом с sample.quadratic")
endif()
