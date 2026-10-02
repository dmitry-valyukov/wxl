#include "item_template.h"

#include <map>
#include <memory>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "../Object.impl.h"
#include "../generated/Microsoft.UI.Xaml.impl.h"

namespace wxl::impl {

namespace {

namespace xaml = winrt::Microsoft::UI::Xaml;

// What the handler of one list reads, and what the next call to set_item_template replaces: the
// function. The handler is put on a list once; its token is kept to take it off when the list leaves.
struct Template {
    ItemBuilder build;
    winrt::event_token token;
};

std::map<void*, std::shared_ptr<Template>>& templates() {
    static std::map<void*, std::shared_ptr<Template>> all;
    return all;
}

winrt::Windows::Foundation::IInspectable element_of(ItemBuilder const& build, winrt::Windows::Foundation::IInspectable const& item) {
    return Object::Impl::as<xaml::UIElement>(build(Object::Impl::wrap<Object>(item)));
}

}  // namespace

void set_item_template(xaml::Controls::ListViewBase const& list, ItemBuilder const& build) {
    void* const key = winrt::get_abi(list);
    auto& all = templates();

    if (auto const found = all.find(key); found != all.end()) {
        // A new function for a list that has the handler already: the containers on screen are filled again.
        found->second->build = build;
        auto const items = list.Items();
        for (uint32_t index = 0; index < items.Size(); ++index) {
            if (auto const container = list.ContainerFromIndex(index).try_as<xaml::Controls::ContentControl>()) {
                container.Content(element_of(build, items.GetAt(index)));
            }
        }
        return;
    }

    auto const state = std::make_shared<Template>();
    state->build = build;
    all[key] = state;
    state->token = list.ContainerContentChanging([state](xaml::Controls::ListViewBase const&,
                                                         xaml::Controls::ContainerContentChangingEventArgs const& args) {
        auto const container = args.ItemContainer().try_as<xaml::Controls::ContentControl>();
        if (!container) return;
        if (args.InRecycleQueue()) {
            container.Content(nullptr);
        } else {
            container.Content(element_of(state->build, args.Item()));
        }
    });
    list.Unloaded([key](auto const&, auto const&) { templates().erase(key); });
}

}  // namespace wxl::impl
