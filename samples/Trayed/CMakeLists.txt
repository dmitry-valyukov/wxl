# Trayed -- чужая программа, живущая в нашей консоли и управляемая из трея.
#
# Линкует wxl::ui, как и остальные примеры, и по той же причине: точка входа
# принадлежит wxl. XAML в нём нет вовсе -- консоль служит окном, а значок
# интерфейсом, -- но wWinMain, Windows App Runtime и пул памяти STA всё равно
# берутся из библиотеки, и начинается приложение всё так же в wxl_launched().
#
# Подсистема GUI -- то, что делает точкой входа собственный wWinMain wxl, и
# то, из-за чего выделенная консоль оказывается своей, а не той, из которой
# программу запустили.

wxl_add_executable(sample.trayed
    main.cpp
    Settings.h
    Settings.cpp
    settings_window.h
    settings_window.cpp
)

# comctl32 ради SetWindowSubclass: окно настроек (окно wxl/WinUI) подменяет
# оконную процедуру, чтобы его кнопка закрытия прятала окно, а не завершала
# приложение.
target_link_libraries(sample.trayed PRIVATE comctl32)
