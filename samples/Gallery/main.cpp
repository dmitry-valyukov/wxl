// WinUI 3 Gallery без XAML — App и MainWindow оригинала: окно строит
// оболочка (Shell.cpp), здесь только точка входа.

#include "Shell.h"

wxl::Teardown wxl_launched() {
    auto window = gallery::createMainWindow();
    window.activate();

    // Оболочка держит окно и страницы; отпустить их надо до остановки пула.
    return [](wxl::TeardownReason) -> std::optional<int> {
        gallery::destroyMainWindow();
        return std::nullopt;
    };
}
