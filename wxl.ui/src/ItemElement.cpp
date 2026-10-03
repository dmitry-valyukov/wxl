#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include "ItemElement.h"
#include "Object.impl.h"
#include "generated/Microsoft.UI.Xaml.Controls.impl.h"

namespace wxl {

namespace xaml = winrt::Microsoft::UI::Xaml;

UIElement itemElement(ListViewBase const& list, Object const& item) {
    winrt::Windows::Foundation::IInspectable native_item{nullptr};
    winrt::copy_from_abi(native_item, item.get_abi());

    // The container's content is a placeholder control (see impl/item_template.cpp) that holds the element. A miss at any step
    // is a null element, which wrap makes an empty wrapper of.
    auto const container = Object::Impl::as<xaml::Controls::ListViewBase>(list).ContainerFromItem(native_item).try_as<xaml::Controls::ContentControl>();
    xaml::UIElement element{nullptr};
    if (container) {
        if (auto const place = container.ContentTemplateRoot().try_as<xaml::Controls::ContentControl>()) {
            element = place.Content().try_as<xaml::UIElement>();
        }
    }
    return Object::Impl::wrap<UIElement>(std::move(element));
}

}  // namespace wxl
