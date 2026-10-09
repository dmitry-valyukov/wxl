#include "item_factory.h"

#include <winrt/Windows.Foundation.h>

#include "../Object.impl.h"
#include <wxl/Microsoft.UI.Xaml.impl.h>
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>
#include "bound_items_source.h"
#include "element_scope.h"

namespace wxl::impl {

namespace {

namespace xaml = winrt::Microsoft::UI::Xaml;
using winrt::Windows::Foundation::IInspectable;

// The element of an item, as the control is given it: an item of ItemsSource in, the element to show it out.
using item_maker = core::function<IInspectable(IInspectable const& item)>;

// The factory: an element made by the function for each item the control asks one for, inside a binding_scope kept
// until the element is given back. An element given back is not kept for another item -- the function is a closure
// over its own item, and makes a new one, which is correct and, for the controls on a scrolling page, cheap enough.
//
// The control is the factory's only holder (ItemTemplate), so what the factory keeps -- the function, the scopes of
// the elements out -- goes with the control.
class element_factory final : public sta_com_object<element_factory, xaml::IElementFactory>
{
public:
    // `containers`: the factory of an ItemsView, which takes an ItemContainer for each item and nothing else.
    element_factory(item_maker make, bool containers) : make_(std::move(make)), containers_(containers) {}

    ~element_factory() { take_out_given_back(); }

    int32_t __stdcall GetElement(void* args, void** result) noexcept final try {
        take_out_given_back();
        auto const& asked = *reinterpret_cast<xaml::ElementFactoryGetArgs const*>(&args);
        com_ptr<element_scope> scope{new element_scope()};
        xaml::UIElement element = scope->bindings.collect([&] { return make_(asked.Data()); }).try_as<xaml::UIElement>();
        if (!element) element = blank();
        // A stale entry at the same address -- an element that went without being given back -- is cut as it is
        // replaced.
        scopes_[identity_of(element)] = std::move(scope);
        *result = winrt::detach_abi(element);
        return 0;
    } catch (...) {
        *result = nullptr;
        return winrt::to_hresult();
    }

    // The element given back: its bindings leave their fields at once, and the element leaves the control's children,
    // since this factory gives out none again -- unless the control has taken it out itself, and then it is simply not
    // found. Not here but when the control next asks for an element, or with the factory: the control may give
    // elements back while it walks its children, and a child taken out mid-walk would make the walk skip one.
    int32_t __stdcall RecycleElement(void* args) noexcept final try {
        auto const& given = *reinterpret_cast<xaml::ElementFactoryRecycleArgs const*>(&args);
        xaml::UIElement const element = given.Element();
        if (!element) return 0;
        scopes_.erase(identity_of(element));
        if (auto const parent = given.Parent().try_as<xaml::Controls::Panel>()) {
            given_back_.push_back({winrt::make_weak(element), winrt::make_weak(parent)});
        }
        return 0;
    } catch (...) {
        return winrt::to_hresult();
    }

private:
    struct given_back {
        winrt::weak_ref<xaml::UIElement> element;
        winrt::weak_ref<xaml::Controls::Panel> parent;
    };

    static void* identity_of(xaml::UIElement const& element) {
        return winrt::get_abi(element.as<winrt::Windows::Foundation::IUnknown>());
    }

    // Held weakly, both: the element and the control it went back to are the control's to keep alive.
    void take_out_given_back() noexcept {
        auto const all = std::exchange(given_back_, {});
        for (given_back const& each : all) {
            try {
                auto const element = each.element.get();
                auto const parent = each.parent.get();
                if (!element || !parent) continue;
                auto const children = parent.Children();
                uint32_t at = 0;
                if (children.IndexOf(element, at)) children.RemoveAt(at);
            } catch (...) {
                // A control that refuses to let a child go keeps it: nothing here can do better.
            }
        }
    }

    // What an item that has nothing to show is shown as: a bound item whose list is gone.
    xaml::UIElement blank() const {
        if (containers_) return xaml::Controls::ItemContainer{};
        return xaml::Controls::Border{};
    }

    item_maker make_;
    bool containers_;
    core::sta_unordered_map<void*, com_ptr<element_scope>> scopes_;
    core::sta_vector<given_back> given_back_;
};

xaml::IElementFactory factory_of(item_maker make, bool containers) {
    com_ptr<element_factory> const made{new element_factory{std::move(make), containers}};
    return made->as_interface<xaml::IElementFactory>();
}

item_maker maker_of(ItemBuilder const& build) {
    return [build](IInspectable const& item) -> IInspectable {
        return Object::Impl::as<xaml::UIElement>(build(Object::Impl::wrap<Object>(item)));
    };
}

}  // namespace

void set_repeater_template(xaml::Controls::ItemsRepeater const& repeater, ItemBuilder const& build) {
    repeater.ItemTemplate(factory_of(maker_of(build), false));
}

void set_items_view_template(xaml::Controls::ItemsView const& view, ItemBuilder const& build) {
    view.ItemTemplate(factory_of(maker_of(build), true));
}

// The template first, so that the first elements the source brings are made by it.
void attach_items(ItemsView const& control, bound_source const& source) {
    xaml::Controls::ItemsView const& view = *Object::Impl::get_typed<ItemsView>(control);
    view.ItemTemplate(factory_of(&bound_element, true));
    view.ItemsSource(source.source().as_inspectable());
}

void attach_items(ItemsRepeater const& control, bound_source const& source) {
    xaml::Controls::ItemsRepeater const& repeater = *Object::Impl::get_typed<ItemsRepeater>(control);
    repeater.ItemTemplate(factory_of(&bound_element, false));
    repeater.ItemsSource(source.source().as_inspectable());
}

}  // namespace wxl::impl
