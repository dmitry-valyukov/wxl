#include "scroll_controller.h"

#include <winrt/Microsoft.UI.Xaml.h>

#include <memory>

#include "../Object.impl.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>

namespace wxl::impl {

namespace {

namespace controls = winrt::Microsoft::UI::Xaml::Controls;

controls::AnnotatedScrollBar native(AnnotatedScrollBar const& bar) {
    return *Object::Impl::get_typed<AnnotatedScrollBar>(bar);
}

// A ScrollView has its ScrollPresenter once its template is applied, which is
// when it is loaded: before that the presenter is null and there is nothing to
// give a controller to. So a view not yet loaded is given the controller at
// its Loaded, once -- the handler takes itself off, and with it lets go of the
// bar.
template <typename Give>
void give_when_loaded(controls::ScrollView const& view, controls::AnnotatedScrollBar const& bar, Give give) {
    if (auto const presenter = view.ScrollPresenter()) {
        give(presenter, bar);
        return;
    }
    auto const token = std::make_shared<winrt::event_token>();
    *token = view.Loaded([bar, give, token](winrt::Windows::Foundation::IInspectable const& sender,
                                            winrt::Microsoft::UI::Xaml::RoutedEventArgs const&) {
        auto const loaded = sender.as<controls::ScrollView>();
        give(loaded.ScrollPresenter(), bar);
        loaded.Loaded(*token);
    });
}

}  // namespace

void set_vertical_scroll_controller(controls::ScrollView const& view, AnnotatedScrollBar const& bar) {
    give_when_loaded(view, native(bar), [](auto const& presenter, auto const& scrollBar) {
        presenter.VerticalScrollController(scrollBar.ScrollController());
    });
}

void set_horizontal_scroll_controller(controls::ScrollView const& view, AnnotatedScrollBar const& bar) {
    give_when_loaded(view, native(bar), [](auto const& presenter, auto const& scrollBar) {
        presenter.HorizontalScrollController(scrollBar.ScrollController());
    });
}

}  // namespace wxl::impl
