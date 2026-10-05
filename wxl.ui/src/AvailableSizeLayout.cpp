#include <algorithm>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include "AvailableSizeLayout.h"
#include "Object.impl.h"
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>

namespace wxl {

namespace xaml = winrt::Microsoft::UI::Xaml;

namespace {

// Объект WinUI раскладки: наследник NonVirtualizingLayout через агрегацию. Поля модели держит по адресу и не владеет
// ими -- адрес снимает страж привязки, когда поле уходит (см. bind ниже).
struct AvailableSizeLayoutObject : xaml::Controls::NonVirtualizingLayoutT<AvailableSizeLayoutObject> {
    // Имя для дерева элементов (Live Visual Tree).
    winrt::hstring GetRuntimeClassName() const { return L"wxl.AvailableSizeLayout"; }

    winrt::Windows::Foundation::Size MeasureOverride(xaml::Controls::NonVirtualizingLayoutContext const& context,
                                                     winrt::Windows::Foundation::Size available) {
        // Сначала поля, потом дети: то, что следует за полями, встаёт до измерения и попадает в этот же проход.
        if (width) {
            width->set(available.Width);
        }
        if (height) {
            height->set(available.Height);
        }
        winrt::Windows::Foundation::Size desired {0, 0};
        for (auto const& child : context.Children()) {
            child.Measure(available);
            auto const size = child.DesiredSize();
            desired.Width = std::max(desired.Width, size.Width);
            desired.Height = std::max(desired.Height, size.Height);
        }
        return desired;
    }

    winrt::Windows::Foundation::Size ArrangeOverride(xaml::Controls::NonVirtualizingLayoutContext const& context,
                                                     winrt::Windows::Foundation::Size final) {
        for (auto const& child : context.Children()) {
            child.Arrange({0, 0, final.Width, final.Height});
        }
        return final;
    }

    core::observable<double>* width = nullptr;
    core::observable<double>* height = nullptr;
};

// То, что привязка оставляет на раскладке: адрес поля. Стража держит только наблюдение самого поля, так что адрес
// снимается, когда поле уходит или его привязки обрывает unbind(), -- раньше, чем он повиснет.
struct field_guard : core::sta_refcounted {
    winrt::com_ptr<AvailableSizeLayoutObject> object;
    core::observable<double>* AvailableSizeLayoutObject::* slot;
    core::observable<double>* field;

    field_guard(winrt::com_ptr<AvailableSizeLayoutObject> o, core::observable<double>* AvailableSizeLayoutObject::* s, core::observable<double>* f)
        : object(std::move(o)), slot(s), field(f) {}

    // Свойство могли привязать заново к другому полю: тогда адрес уже не этого стража.
    ~field_guard() {
        if ((*object).*slot == field) {
            (*object).*slot = nullptr;
        }
    }
};

}  // namespace

class AvailableSizeLayout::Impl : public base_t::Impl {
public:
    explicit Impl(winrt::com_ptr<AvailableSizeLayoutObject> const& object)
        : base_t::Impl(object.as<xaml::Controls::Layout>()), object_(object) {}

    static winrt::com_ptr<AvailableSizeLayoutObject> const& of(Object::Impl* impl) { return static_cast<Impl*>(impl)->object_; }

    static void bind(AvailableSizeLayout const& layout, core::observable<double>& field,
                     core::observable<double>* AvailableSizeLayoutObject::* slot) {
        auto const& object = of(layout.impl());
        (*object).*slot = &field;
        core::intrusive_ptr<field_guard> guard {new field_guard {object, slot, &field}, /*add_ref=*/false};
        field.watch_for_binding([guard = std::move(guard)](double const&) noexcept {});
    }

private:
    // Сам объект держит inspectable_ базы; здесь -- его реализация, до которой из проекции не дотянуться.
    winrt::com_ptr<AvailableSizeLayoutObject> object_;
};

AvailableSizeLayout::AvailableSizeLayout() : base_t(new Impl {winrt::make_self<AvailableSizeLayoutObject>()}) {}

AvailableSizeLayout::AvailableSizeLayout(Impl* impl) noexcept : base_t(impl) {}

void AvailableSizeLayout::bind_width(AvailableSizeLayout const& layout, core::observable<double>& field) {
    Impl::bind(layout, field, &AvailableSizeLayoutObject::width);
}

void AvailableSizeLayout::bind_height(AvailableSizeLayout const& layout, core::observable<double>& field) {
    Impl::bind(layout, field, &AvailableSizeLayoutObject::height);
}

}  // namespace wxl
