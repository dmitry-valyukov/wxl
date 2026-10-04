#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.UI.h>

#include <algorithm>
#include <cmath>

#include "SplitPanel.h"
#include "Object.impl.h"
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>

namespace wxl {

namespace xaml = winrt::Microsoft::UI::Xaml;
namespace foundation = winrt::Windows::Foundation;

namespace {

// Объект WinUI панели. Детей у него три: две части и разделитель -- последним,
// чтобы лежать поверх частей. Состояние перетаскивания живёт здесь же: обёртка
// wxl своего ничего не несёт.
struct SplitPanelObject : xaml::Controls::PanelT<SplitPanelObject> {
    SplitPanelObject() {
        grip_.Fill(xaml::Media::SolidColorBrush {winrt::Windows::UI::Color {0, 0, 0, 0}});
        showCursor();
        grip_.PointerPressed({this, &SplitPanelObject::pressed});
        grip_.PointerMoved({this, &SplitPanelObject::moved});
        grip_.PointerReleased({this, &SplitPanelObject::released});
        grip_.PointerCaptureLost({this, &SplitPanelObject::lost});
        Children().Append(grip_);
    }

    // Имя для дерева элементов (Live Visual Tree): без него cppwinrt отвечает
    // именем первого интерфейса из implements.
    winrt::hstring GetRuntimeClassName() const { return L"wxl.SplitPanel"; }

    void showCursor() {
        grip_.as<xaml::IUIElementProtected>().ProtectedCursor(winrt::Microsoft::UI::Input::InputSystemCursor::Create(
            vertical_ ? winrt::Microsoft::UI::Input::InputSystemCursorShape::SizeNorthSouth
                      : winrt::Microsoft::UI::Input::InputSystemCursorShape::SizeWestEast));
    }

    // Часть встаёт перед разделителем; прежняя уходит из детей.
    void place(xaml::UIElement& slot, xaml::UIElement const& value) {
        auto const children = Children();
        uint32_t at = 0;
        if (slot && children.IndexOf(slot, at)) {
            children.RemoveAt(at);
        }
        slot = value;
        if (slot && children.IndexOf(grip_, at)) {
            children.InsertAt(at, slot);
        }
    }

    // Размеры и точки -- по оси панели и поперёк неё.
    float along(foundation::Size size) const { return vertical_ ? size.Height : size.Width; }
    float across(foundation::Size size) const { return vertical_ ? size.Width : size.Height; }
    float along(foundation::Point point) const { return vertical_ ? point.Y : point.X; }
    float along(winrt::Windows::Foundation::Numerics::float2 size) const { return vertical_ ? size.y : size.x; }

    foundation::Size size(float main, float cross) const {
        return vertical_ ? foundation::Size {cross, main} : foundation::Size {main, cross};
    }

    foundation::Rect rect(float offset, float main, float cross) const {
        return vertical_ ? foundation::Rect {0, offset, cross, main} : foundation::Rect {offset, 0, main, cross};
    }

    // Сколько pane помещается в данный размер: зазор и content остаются
    // видны, пока места хватает на зазор.
    float fitted(float extent) const {
        if (!std::isfinite(extent)) {
            return length_;
        }
        return std::clamp(length_, 0.0f, std::max(0.0f, extent - spacing_));
    }

    foundation::Size MeasureOverride(foundation::Size available) {
        float const extent = along(available);
        float const cross = across(available);
        float const length = fitted(extent);
        float const rest = std::isfinite(extent) ? std::max(0.0f, extent - length - spacing_) : extent;
        float main = length + spacing_;
        float thickness = 0;
        if (pane_) {
            pane_.Measure(size(length, cross));
            thickness = std::max(thickness, across(pane_.DesiredSize()));
        }
        if (content_) {
            content_.Measure(size(rest, cross));
            main += along(content_.DesiredSize());
            thickness = std::max(thickness, across(content_.DesiredSize()));
        }
        grip_.Measure(size(spacing_, cross));
        return size(main, thickness);
    }

    foundation::Size ArrangeOverride(foundation::Size final) {
        float const extent = along(final);
        float const cross = across(final);
        float const length = fitted(extent);
        float const rest = std::max(0.0f, extent - length - spacing_);
        float const paneAt = trailing_ ? rest + spacing_ : 0;
        float const gripAt = trailing_ ? rest : length;
        float const contentAt = trailing_ ? 0 : length + spacing_;
        if (pane_) {
            pane_.Arrange(rect(paneAt, length, cross));
        }
        grip_.Arrange(rect(gripAt, spacing_, cross));
        if (content_) {
            content_.Arrange(rect(contentAt, rest, cross));
        }
        return final;
    }

    float leastOf(xaml::UIElement const& element) const {
        auto const framework = element.try_as<xaml::FrameworkElement>();
        return framework ? static_cast<float>(vertical_ ? framework.MinHeight() : framework.MinWidth()) : 0.0f;
    }

    // Граница не заходит за MinWidth (MinHeight) ни одной из частей; если обе
    // не помещаются, pane берёт своё.
    float clamped(float length) const {
        float const least = pane_ ? leastOf(pane_) : 0.0f;
        float const most = along(ActualSize()) - spacing_ - (content_ ? leastOf(content_) : 0.0f);
        return std::max(least, std::min(length, most));
    }

    void pressed(foundation::IInspectable const&, xaml::Input::PointerRoutedEventArgs const& args) {
        if (grip_.CapturePointer(args.Pointer())) {
            dragging_ = true;
            from_ = along(args.GetCurrentPoint(*this).Position());
            start_ = fitted(along(ActualSize()));
            args.Handled(true);
        }
    }

    void moved(foundation::IInspectable const&, xaml::Input::PointerRoutedEventArgs const& args) {
        if (!dragging_) {
            return;
        }
        // Pane после content растёт, когда границу тянут назад.
        float const shift = along(args.GetCurrentPoint(*this).Position()) - from_;
        float const length = clamped(trailing_ ? start_ - shift : start_ + shift);
        if (length != length_) {
            length_ = length;
            InvalidateMeasure();
        }
        args.Handled(true);
    }

    void released(foundation::IInspectable const&, xaml::Input::PointerRoutedEventArgs const& args) {
        if (dragging_) {
            dragging_ = false;
            grip_.ReleasePointerCapture(args.Pointer());
            args.Handled(true);
        }
    }

    void lost(foundation::IInspectable const&, xaml::Input::PointerRoutedEventArgs const&) {
        dragging_ = false;
    }

    xaml::UIElement pane_ {nullptr};
    xaml::UIElement content_ {nullptr};
    xaml::Shapes::Rectangle grip_;

    float length_ = 320;  // как OpenPaneLength у SplitView
    float spacing_ = 8;
    bool vertical_ = false;
    bool trailing_ = false;  // pane после content

    bool dragging_ = false;
    float from_ = 0;   // где нажали, по оси панели
    float start_ = 0;  // размер pane в тот миг
};

}  // namespace

class SplitPanel::Impl : public base_t::Impl {
public:
    explicit Impl(winrt::com_ptr<SplitPanelObject> const& object)
        : base_t::Impl(object.as<xaml::Controls::Panel>()), object_(object.get()) {}

    // Реализация объекта обёртки.
    static SplitPanelObject& of(Object::Impl* impl) { return *static_cast<Impl*>(impl)->object_; }

private:
    // Сам объект держит inspectable_ базы; здесь -- его реализация, до
    // которой из проекции не дотянуться.
    SplitPanelObject* object_;
};

SplitPanel::SplitPanel() : base_t(new Impl {winrt::make_self<SplitPanelObject>()}) {}

SplitPanel::SplitPanel(Impl* impl) noexcept : base_t(impl) {}

void SplitPanel::pane(UIElement const& value) const {
    SplitPanelObject& object = Impl::of(impl());
    object.place(object.pane_, value ? static_cast<xaml::UIElement const&>(*Object::Impl::get_typed<UIElement>(value))
                                     : xaml::UIElement {nullptr});
    object.InvalidateMeasure();
}

UIElement SplitPanel::pane() const {
    return Object::Impl::wrap<UIElement>(xaml::UIElement {Impl::of(impl()).pane_});
}

void SplitPanel::content(UIElement const& value) const {
    SplitPanelObject& object = Impl::of(impl());
    object.place(object.content_, value ? static_cast<xaml::UIElement const&>(*Object::Impl::get_typed<UIElement>(value))
                                        : xaml::UIElement {nullptr});
    object.InvalidateMeasure();
}

UIElement SplitPanel::content() const {
    return Object::Impl::wrap<UIElement>(xaml::UIElement {Impl::of(impl()).content_});
}

void SplitPanel::openPaneLength(double value) const {
    SplitPanelObject& object = Impl::of(impl());
    object.length_ = static_cast<float>(value);
    object.InvalidateMeasure();
}

double SplitPanel::openPaneLength() const {
    return Impl::of(impl()).length_;
}

void SplitPanel::orientation(Orientation value) const {
    SplitPanelObject& object = Impl::of(impl());
    object.vertical_ = value == Orientation::Vertical;
    object.showCursor();
    object.InvalidateMeasure();
}

Orientation SplitPanel::orientation() const {
    return Impl::of(impl()).vertical_ ? Orientation::Vertical : Orientation::Horizontal;
}

void SplitPanel::panePlacement(SplitViewPanePlacement value) const {
    SplitPanelObject& object = Impl::of(impl());
    object.trailing_ = value == SplitViewPanePlacement::Right;
    object.InvalidateArrange();
}

SplitViewPanePlacement SplitPanel::panePlacement() const {
    return Impl::of(impl()).trailing_ ? SplitViewPanePlacement::Right : SplitViewPanePlacement::Left;
}

void SplitPanel::spacing(double value) const {
    SplitPanelObject& object = Impl::of(impl());
    object.spacing_ = static_cast<float>(value);
    object.InvalidateMeasure();
}

double SplitPanel::spacing() const {
    return Impl::of(impl()).spacing_;
}

void SplitPanel::setPositional(UIElement const& value) const {
    if (Impl::of(impl()).pane_) {
        content(value);
    } else {
        pane(value);
    }
}

}  // namespace wxl
