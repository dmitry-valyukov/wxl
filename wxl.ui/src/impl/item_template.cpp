#include "item_template.h"

#include <map>
#include <memory>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "../Object.impl.h"
#include "../generated/Microsoft.UI.Xaml.impl.h"
#include "conversions.h"

namespace wxl::impl {

namespace {

namespace xaml = winrt::Microsoft::UI::Xaml;

// What the handler of one list reads, and what the next call to a setter replaces: the function that
// makes the element of an item and the margin of every container. The handler is put on a list once.
struct Template {
    ItemBuilder build;
    bool has_margin = false;
    xaml::Thickness margin{};
    winrt::event_token token;
};

std::map<void*, std::shared_ptr<Template>>& templates() {
    static std::map<void*, std::shared_ptr<Template>> all;
    return all;
}

winrt::Windows::Foundation::IInspectable element_of(ItemBuilder const& build, winrt::Windows::Foundation::IInspectable const& item) {
    return Object::Impl::as<xaml::UIElement>(build(Object::Impl::wrap<Object>(item)));
}

// The state of a list, made and hooked up on the first call.
std::shared_ptr<Template> state_of(xaml::Controls::ListViewBase const& list) {
    void* const key = winrt::get_abi(list);
    auto& all = templates();
    if (auto const found = all.find(key); found != all.end()) {
        return found->second;
    }

    auto const state = std::make_shared<Template>();
    all[key] = state;
    state->token = list.ContainerContentChanging([state](xaml::Controls::ListViewBase const&,
                                                         xaml::Controls::ContainerContentChangingEventArgs const& args) {
        auto const container = args.ItemContainer().try_as<xaml::Controls::ContentControl>();
        if (!container) return;
        if (state->has_margin) container.Margin(state->margin);
        if (!state->build) return;
        if (args.InRecycleQueue()) {
            if (auto const place = container.ContentTemplateRoot().try_as<xaml::Controls::ContentControl>()) place.Content(nullptr);
            return;
        }
        // The framework puts the item into the container after the first call; the element has to
        // replace it after that, which is what the callback of the next phase is for.
        args.RegisterUpdateCallback(1, [state](xaml::Controls::ListViewBase const&,
                                              xaml::Controls::ContainerContentChangingEventArgs const& later) {
            auto const target = later.ItemContainer().try_as<xaml::Controls::ContentControl>();
            if (!target || !state->build) return;
            if (auto const place = target.ContentTemplateRoot().try_as<xaml::Controls::ContentControl>()) {
                place.Content(element_of(state->build, later.Item()));
            }
        });
    });
    list.Unloaded([key](auto const&, auto const&) { templates().erase(key); });
    return state;
}

}  // namespace

void set_item_template(xaml::Controls::ListViewBase const& list, ItemBuilder const& build) {
    auto const state = state_of(list);
    state->build = build;

    // The container shows the item through a template, and the template here is a place for the element: a
    // content control the function fills in when the container is prepared. (An element put straight into
    // the Content of a container made for an item is not shown.)
    if (!list.ItemTemplate()) {
        list.ItemTemplate(xaml::Markup::XamlReader::Load(
                              L"<DataTemplate xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'>"
                              L"<ContentControl HorizontalContentAlignment='Stretch' VerticalContentAlignment='Stretch'/>"
                              L"</DataTemplate>")
                              .as<xaml::DataTemplate>());
    }

    // A new function for a list that has items on screen already: the containers are filled again.
    auto const items = list.Items();
    for (uint32_t index = 0; index < items.Size(); ++index) {
        if (auto const container = list.ContainerFromIndex(index).try_as<xaml::Controls::ContentControl>()) {
            if (auto const place = container.ContentTemplateRoot().try_as<xaml::Controls::ContentControl>()) {
                place.Content(element_of(build, items.GetAt(index)));
            }
        }
    }
}

void set_item_margin(xaml::Controls::ListViewBase const& list, Thickness const& margin) {
    auto const state = state_of(list);
    state->has_margin = true;
    state->margin = to_winrt(margin);

    auto const items = list.Items();
    for (uint32_t index = 0; index < items.Size(); ++index) {
        if (auto const container = list.ContainerFromIndex(index).try_as<xaml::Controls::ContentControl>()) {
            container.Margin(state->margin);
        }
    }
}

}  // namespace wxl::impl
