#include "item_factory.h"

#include <winrt/Windows.Foundation.h>

#include "../Object.impl.h"
#include <wxl/Microsoft.UI.Xaml.impl.h>

namespace wxl::impl {

namespace {

namespace xaml = winrt::Microsoft::UI::Xaml;

// The factory: an element made by the function for each item the control asks one for. An element given
// back is not kept for another item -- the function makes a new one, which is correct and, for the
// controls on a scrolling page, cheap enough.
struct Factory : winrt::implements<Factory, xaml::IElementFactory> {
    explicit Factory(ItemBuilder build) : build_(std::move(build)) {}

    xaml::UIElement GetElement(xaml::ElementFactoryGetArgs const& args) {
        return Object::Impl::as<xaml::UIElement>(build_(Object::Impl::wrap<Object>(args.Data())));
    }

    void RecycleElement(xaml::ElementFactoryRecycleArgs const&) {}

private:
    ItemBuilder build_;
};

}  // namespace

void set_repeater_template(xaml::Controls::ItemsRepeater const& repeater, ItemBuilder const& build) {
    repeater.ItemTemplate(winrt::make<Factory>(build));
}

void set_items_view_template(xaml::Controls::ItemsView const& view, ItemBuilder const& build) {
    view.ItemTemplate(winrt::make<Factory>(build));
}

}  // namespace wxl::impl
