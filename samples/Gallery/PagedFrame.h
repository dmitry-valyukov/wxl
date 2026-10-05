#pragma once

// Рамка (Frame), страницы которой делают функции: образец оригинала навигирует Frame по типам XAML-страниц,
// здесь страница -- Page самой платформы (navigatePage), а её содержимое строит функция. Возврат (GoBack) делает
// страницу заново пустой, и функция наполняет её снова.

#include <functional>
#include <memory>
#include <vector>

#include "Navigation.h"

namespace gallery {

class PagedFrame {
public:
    using Builder = std::function<wxl::UIElement()>;

    explicit PagedFrame(wxl::Frame frame);

    wxl::Frame const& frame() const { return state_->frame; }

    /// Переход на новую страницу, которую наполнит `build`; с переходом рамки по умолчанию.
    wxl::Page forward(Builder build) const;

    /// То же с выбранным для этого перехода (SuppressNavigationTransitionInfo и др.).
    wxl::Page forward(Builder build, wxl::NavigationTransitionInfo const& info) const;

    /// Шаг назад: страница прежней функции делается заново.
    wxl::Page back() const;

    /// Сколько страниц позади (Frame.BackStackDepth).
    int depth() const;

private:
    struct State {
        wxl::Frame frame;
        std::vector<Builder> stack;
    };
    std::shared_ptr<State> state_;
};

}  // namespace gallery
