#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include "CustomLayout.h"
#include "Collection.impl.h"
#include "Object.impl.h"
#include "generated/Microsoft.UI.Xaml.Controls.impl.h"
#include "impl/conversions.h"

namespace wxl {

namespace xaml = winrt::Microsoft::UI::Xaml;

namespace {

// Объект WinUI раскладки: наследник NonVirtualizingLayout через агрегацию,
// обе перегрузки отдают работу функциям, которые принесла обёртка.
struct CustomLayoutObject : xaml::Controls::NonVirtualizingLayoutT<CustomLayoutObject> {
    CustomLayoutObject(CustomLayout::Measure measure, CustomLayout::Arrange arrange)
        : measure_(std::move(measure)), arrange_(std::move(arrange)) {}

    // Имя для дерева элементов (Live Visual Tree).
    winrt::hstring GetRuntimeClassName() const { return L"wxl.CustomLayout"; }

    winrt::Windows::Foundation::Size MeasureOverride(xaml::Controls::NonVirtualizingLayoutContext const& context,
                                                     winrt::Windows::Foundation::Size available) {
        return impl::to_winrt(measure_(children(context), impl::from_winrt(available)));
    }

    winrt::Windows::Foundation::Size ArrangeOverride(xaml::Controls::NonVirtualizingLayoutContext const& context,
                                                     winrt::Windows::Foundation::Size final) {
        return impl::to_winrt(arrange_(children(context), impl::from_winrt(final)));
    }

    void invalidate() { InvalidateMeasure(); }

private:
    static Collection<UIElement> children(xaml::Controls::NonVirtualizingLayoutContext const& context) {
        return Object::Impl::wrap<Collection<UIElement>>(context.Children());
    }

    CustomLayout::Measure measure_;
    CustomLayout::Arrange arrange_;
};

}  // namespace

class CustomLayout::Impl : public base_t::Impl {
public:
    explicit Impl(winrt::com_ptr<CustomLayoutObject> const& object)
        : base_t::Impl(object.as<xaml::Controls::Layout>()), object_(object.get()) {}

    static CustomLayoutObject& of(Object::Impl* impl) { return *static_cast<Impl*>(impl)->object_; }

private:
    // Сам объект держит inspectable_ базы; здесь -- его реализация, до
    // которой из проекции не дотянуться.
    CustomLayoutObject* object_;
};

CustomLayout::CustomLayout(Measure measure, Arrange arrange)
    : base_t(new Impl {winrt::make_self<CustomLayoutObject>(std::move(measure), std::move(arrange))}) {}

CustomLayout::CustomLayout(Impl* impl) noexcept : base_t(impl) {}

void CustomLayout::invalidate() const {
    Impl::of(impl()).invalidate();
}

}  // namespace wxl
