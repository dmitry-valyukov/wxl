#include "navigation_view_search.h"

#include "../Object.impl.h"
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>

namespace wxl::impl {

void set_auto_suggest_box(winrt::Microsoft::UI::Xaml::Controls::NavigationView const& view, core::nullable<AutoSuggestBox> const& box) {
    view.AutoSuggestBox(box ? Object::Impl::as<winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBox>(*box)
                            : winrt::Microsoft::UI::Xaml::Controls::AutoSuggestBox{nullptr});
}

}  // namespace wxl::impl
