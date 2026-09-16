# Проба своего окна на композиторе -- открывает wxl::CompositionWindow. Как
# любое приложение на wxl: линкует только wxl::ui, проекции cppwinrt не
# видит. WIN32 -- GUI-подсистема, оттого точкой входа служит wWinMain самой wxl.
add_executable(sample.composition-probe WIN32
    main.cpp
)

wxl_set_target_folders(sample.composition-probe)

target_link_libraries(sample.composition-probe PRIVATE wxl::ui)

# Загрузчик Windows App Runtime ищет пакет платформы при старте и обязан лежать
# рядом с исполняемым файлом.
set(_wxl_bootstrap_dll
    "${WXL_WINDOWSAPPSDK_FOUNDATION_DIR}/runtimes/win-x64/native/Microsoft.WindowsAppRuntime.Bootstrap.dll")

if(EXISTS "${_wxl_bootstrap_dll}")
    add_custom_command(TARGET sample.composition-probe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_wxl_bootstrap_dll}" "$<TARGET_FILE_DIR:sample.composition-probe>"
        COMMENT "Копирование загрузчика Windows App Runtime рядом с sample.composition-probe")
endif()

# Win2D (Microsoft.Graphics.Canvas) активируется неупакованно из своей нативной
# DLL -- та должна лежать рядом с exe, иначе CanvasDevice/CanvasBitmap не
# создаются. Так же, как загрузчик выше.
set(_wxl_win2d_dll
    "D:/Users/Dima/.nuget/packages/microsoft.graphics.win2d/1.4.0/runtimes/win-x64/native/Microsoft.Graphics.Canvas.dll")

if(EXISTS "${_wxl_win2d_dll}")
    add_custom_command(TARGET sample.composition-probe POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_wxl_win2d_dll}" "$<TARGET_FILE_DIR:sample.composition-probe>"
        COMMENT "Копирование Microsoft.Graphics.Canvas.dll (Win2D) рядом с sample.composition-probe")
endif()
