# Стенд освещения: приложение на wxl, на котором отрабатываются способы
# показать объём светом. Слева сцена опыта, справа его настройки; опыт --
# отдельный .cpp, строка в каталоге main.cpp.
#
# Свет, эффекты и тени композитора берутся обёртками wxl (профиль full).
# Direct2D и Direct3D приложение зовёт само: рисование в DrawingSurface --
# его дело, и заголовки этих API оно включает раньше заголовков wxl.
wxl_add_executable(sandbox.lighting-lab
    Lab.h
    pch.h
    main.cpp
    Controls.cpp
    Failure.cpp
    NormalMaps.cpp
    MaterialPage.cpp
    RevealPage.cpp
    Direct2DPage.cpp
    ShaderPage.cpp
    ShadowPage.cpp
    ScenesPage.cpp
)

# Failure.cpp читает отказ системы из исключения проекции cppwinrt, а
# ScenesPage.cpp строит сцену типами, у которых обёрток wxl нет: обоим нужна
# сама проекция, та же, с которой собрана wxl.ui.
target_include_directories(sandbox.lighting-lab PRIVATE ${WXL_CPPWINRT_PROJECTION_DIR})
target_link_libraries(sandbox.lighting-lab PRIVATE
    Microsoft::CppWinRT d2d1.lib d3d11.lib dxguid.lib d3dcompiler.lib dwrite.lib)
