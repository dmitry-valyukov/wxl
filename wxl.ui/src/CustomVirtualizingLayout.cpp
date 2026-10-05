#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

#include "CustomVirtualizingLayout.h"
#include "Object.impl.h"
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>
#include "impl/conversions.h"

namespace wxl {

namespace xaml = winrt::Microsoft::UI::Xaml;

namespace {

// The WinUI object of the layout: a VirtualizingLayout by aggregation, both overrides
// hand the work to the functions the wrapper brought.
struct CustomVirtualizingLayoutObject : xaml::Controls::VirtualizingLayoutT<CustomVirtualizingLayoutObject> {
    CustomVirtualizingLayoutObject(CustomVirtualizingLayout::Measure measure, CustomVirtualizingLayout::Arrange arrange,
                                  CustomVirtualizingLayout::ItemsChanged itemsChanged)
        : measure_(std::move(measure)), arrange_(std::move(arrange)), itemsChanged_(std::move(itemsChanged)) {}

    // The name in the element tree (Live Visual Tree).
    winrt::hstring GetRuntimeClassName() const { return L"wxl.CustomVirtualizingLayout"; }

    winrt::Windows::Foundation::Size MeasureOverride(xaml::Controls::VirtualizingLayoutContext const& context,
                                                     winrt::Windows::Foundation::Size available) {
        return impl::to_winrt(measure_(Object::Impl::wrap<VirtualizingLayoutContext>(context), impl::from_winrt(available)));
    }

    winrt::Windows::Foundation::Size ArrangeOverride(xaml::Controls::VirtualizingLayoutContext const& context,
                                                     winrt::Windows::Foundation::Size final) {
        return impl::to_winrt(arrange_(Object::Impl::wrap<VirtualizingLayoutContext>(context), impl::from_winrt(final)));
    }

    void OnItemsChangedCore(xaml::Controls::VirtualizingLayoutContext const& context, winrt::Windows::Foundation::IInspectable const&,
                            winrt::Microsoft::UI::Xaml::Interop::NotifyCollectionChangedEventArgs const&) {
        if (itemsChanged_) {
            itemsChanged_(Object::Impl::wrap<VirtualizingLayoutContext>(context));
        }
        // What the base class does with a change of the items, and all it does.
        InvalidateMeasure();
    }

    void invalidate() { InvalidateMeasure(); }

private:
    CustomVirtualizingLayout::Measure measure_;
    CustomVirtualizingLayout::Arrange arrange_;
    CustomVirtualizingLayout::ItemsChanged itemsChanged_;
};

}  // namespace

class CustomVirtualizingLayout::Impl : public base_t::Impl {
public:
    explicit Impl(winrt::com_ptr<CustomVirtualizingLayoutObject> const& object)
        : base_t::Impl(object.as<xaml::Controls::Layout>()), object_(object.get()) {}

    static CustomVirtualizingLayoutObject& of(Object::Impl* impl) { return *static_cast<Impl*>(impl)->object_; }

private:
    // The object itself is held by the inspectable_ of the base; this is its implementation,
    // which the projection cannot reach.
    CustomVirtualizingLayoutObject* object_;
};

CustomVirtualizingLayout::CustomVirtualizingLayout(Measure measure, Arrange arrange)
    : CustomVirtualizingLayout(std::move(measure), std::move(arrange), ItemsChanged{}) {}

CustomVirtualizingLayout::CustomVirtualizingLayout(Measure measure, Arrange arrange, ItemsChanged itemsChanged)
    : base_t(new Impl {winrt::make_self<CustomVirtualizingLayoutObject>(std::move(measure), std::move(arrange), std::move(itemsChanged))}) {}

CustomVirtualizingLayout::CustomVirtualizingLayout(Impl* impl) noexcept : base_t(impl) {}

void CustomVirtualizingLayout::invalidate() const {
    Impl::of(impl()).invalidate();
}

}  // namespace wxl
