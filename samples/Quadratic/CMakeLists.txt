# Квадратное уравнение: модель на observable и интерфейс одним выражением в окне
# CompositionWindow с фоном плиткой, всё в main.cpp. Как любое приложение на wxl,
# линкует только wxl::ui; WIN32 делает точкой входа wWinMain самой wxl.

wxl_add_executable(sample.quadratic
    main.cpp
)

# Иконка приложения -- в самом exe: её показывает Проводник, её же wxl
# ставит окну, и копировать .ico рядом не нужно.
wxl_target_icon(sample.quadratic app.ico)

# Картинка фона ищется рядом с исполняемым файлом. Каждый файл папки --
# выход сборки со своей зависимостью, поэтому правка картинки доезжает до
# exe и без перелинковки.
wxl_target_assets(sample.quadratic Assets)
