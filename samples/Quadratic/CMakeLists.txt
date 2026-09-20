# Квадратное уравнение: модель на observable и интерфейс одним выражением в окне
# CompositionWindow с фоном плиткой, всё в main.cpp. Как любое приложение на wxl,
# линкует только wxl::ui; WIN32 делает точкой входа wWinMain самой wxl.

wxl_add_executable(sample.quadratic
    main.cpp
)

wxl_target_icon(sample.quadratic app.ico)
wxl_target_assets(sample.quadratic Assets)
