#include "item_template.h"

#include <map>
#include <memory>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Windows.Foundation.Collections.h>

#include "../Object.impl.h"
#include <wxl/Microsoft.UI.Xaml.impl.h>
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>
#include "bound_items_source.h"
#include "conversions.h"
#include "element_scope.h"

namespace wxl::impl {

namespace {

namespace xaml = winrt::Microsoft::UI::Xaml;
using winrt::Windows::Foundation::IInspectable;

// The element of an item, as the control is given it: an item of ItemsSource in, the element to show it out.
using item_maker = core::function<IInspectable(IInspectable const& item)>;

// What the handler of one list reads, and what the next call to a setter replaces: the function that makes the element
// of an item and the margin of every container. The handler is put on a list once, and holds this: the list owns its
// template state through the handler.
struct Template {
    core::nullable<item_maker> make;
    bool has_margin = false;
    xaml::Thickness margin{};
    core::nullable<ItemContainerPreset::dress_t> dress;
    // The containers dressed so far, by identity: a container the control reuses for another item comes back
    // through ContainerContentChanging and is not dressed again -- a preset holds bindings and handlers, which must
    // not double. A weak reference tells a container that is gone from a new one at the same address.
    std::map<void*, winrt::weak_ref<xaml::Controls::Primitives::SelectorItem>> dressed;
    winrt::event_token token;
};

// Dresses a container the first time it is seen. Making the containers here instead (ChoosingItemContainer) was
// tried and refused: the control asks for a container of an item more than once and put the item into both.
void dress_once(Template& state, xaml::Controls::Primitives::SelectorItem const& container) {
    if (!state.dress.has_value()) return;
    void* const key = winrt::get_abi(container);
    if (auto const found = state.dressed.find(key); found != state.dressed.end()) {
        if (found->second.get() == container) return;
        state.dressed.erase(found);
    }
    (*state.dress)(Object::Impl::wrap<SelectorItem>(container));
    state.dressed[key] = winrt::make_weak(container);
}

// The state of each list, found by the list's address, but owned by the list's own handler: an entry here keeps
// nothing alive, and an entry whose list is gone -- one that died unloaded or never loaded -- is told from a new list
// at the same address by its weak reference.
struct Indexed {
    winrt::weak_ref<xaml::Controls::ListViewBase> list;
    std::weak_ptr<Template> state;
};

std::map<void*, Indexed>& templates() {
    static std::map<void*, Indexed> all;
    return all;
}

// The place a container shows its element in: the content control of the template set_item_template gives the list.
xaml::Controls::ContentControl place_of(xaml::Controls::ContentControl const& container) {
    return container.ContentTemplateRoot().try_as<xaml::Controls::ContentControl>();
}

// The bindings of the element a place shows, kept in the place itself -- its Tag, which no application sees -- so
// that they live no longer than the container does.
com_ptr<element_scope> scope_of(xaml::Controls::ContentControl const& place) {
    if (auto found = own_object<element_scope>(winrt::get_abi(place.Tag()))) return found;
    com_ptr<element_scope> made{new element_scope()};
    place.Tag(made->as_inspectable());
    return made;
}

// The element of an item, put in its place: the bindings of the element the place showed before are cut, and those of
// the new one are collected while it is built.
void show_item(Template const& state, xaml::Controls::ContentControl const& place, IInspectable const& item) {
    auto const scope = scope_of(place);
    scope->bindings.cut();
    place.Content(nullptr);
    if (!state.make) return;
    place.Content(scope->bindings.collect([&] { return (*state.make)(item); }));
}

// The place given back with its container: the element leaves it, and its bindings leave their fields.
void clear_place(xaml::Controls::ContentControl const& place) {
    if (auto const scope = own_object<element_scope>(winrt::get_abi(place.Tag()))) scope->bindings.cut();
    place.Content(nullptr);
}

// The state of a list, made and hooked up on the first call.
std::shared_ptr<Template> state_of(xaml::Controls::ListViewBase const& list) {
    void* const key = winrt::get_abi(list);
    auto& all = templates();
    if (auto const found = all.find(key); found != all.end()) {
        if (auto state = found->second.state.lock(); state && found->second.list.get() == list) return state;
    }
    std::erase_if(all, [](auto const& entry) { return entry.second.state.expired(); });

    auto const state = std::make_shared<Template>();
    state->token = list.ContainerContentChanging([state](xaml::Controls::ListViewBase const&,
                                                         xaml::Controls::ContainerContentChangingEventArgs const& args) {
        auto const container = args.ItemContainer().try_as<xaml::Controls::ContentControl>();
        if (!container) return;
        if (state->has_margin) container.Margin(state->margin);
        if (auto const item = container.try_as<xaml::Controls::Primitives::SelectorItem>()) dress_once(*state, item);
        if (!state->make) return;
        if (args.InRecycleQueue()) {
            if (auto const place = place_of(container)) clear_place(place);
            return;
        }
        // The framework puts the item into the container after the first call; the element has to
        // replace it after that, which is what the callback of the next phase is for.
        args.RegisterUpdateCallback(1, [state](xaml::Controls::ListViewBase const&,
                                              xaml::Controls::ContainerContentChangingEventArgs const& later) {
            auto const target = later.ItemContainer().try_as<xaml::Controls::ContentControl>();
            if (!target) return;
            if (auto const place = place_of(target)) show_item(*state, place, later.Item());
        });
    });
    all[key] = Indexed{winrt::make_weak(list), state};
    return state;
}

// The function of the list in place: the template that gives each container a place for the element, if the list has
// none yet, and the containers on screen already filled again.
void set_maker(xaml::Controls::ListViewBase const& list, item_maker make) {
    auto const state = state_of(list);
    state->make = std::move(make);

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

    auto const items = list.Items();
    for (uint32_t index = 0; index < items.Size(); ++index) {
        if (auto const container = list.ContainerFromIndex(index).try_as<xaml::Controls::ContentControl>()) {
            if (auto const place = place_of(container)) show_item(*state, place, items.GetAt(index));
        }
    }
}

}  // namespace

void set_item_template(xaml::Controls::ListViewBase const& list, ItemBuilder const& build) {
    set_maker(list, [build](IInspectable const& item) -> IInspectable {
        return Object::Impl::as<xaml::UIElement>(build(Object::Impl::wrap<Object>(item)));
    });
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

void set_item_container_style(xaml::Controls::ListViewBase const& list, ItemContainerPreset const& preset) {
    auto const state = state_of(list);
    state->dress = preset.dress();

    // The containers already made: dressed now, once. A new preset on a list whose containers wear the old one
    // dresses them over it -- the preset adds, nothing takes a value back.
    auto const items = list.Items();
    for (uint32_t index = 0; index < items.Size(); ++index) {
        if (auto const container = list.ContainerFromIndex(index).try_as<xaml::Controls::Primitives::SelectorItem>()) {
            dress_once(*state, container);
        }
    }
}

// The template first, so that the first containers the source brings are filled by it.
void attach_items(ListViewBase const& control, bound_source const& source) {
    xaml::Controls::ListViewBase const& list = *Object::Impl::get_typed<ListViewBase>(control);
    set_maker(list, &bound_element);
    list.ItemsSource(source.source().as_inspectable());
}

}  // namespace wxl::impl
