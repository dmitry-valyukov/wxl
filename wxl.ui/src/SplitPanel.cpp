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
#include "generated/Microsoft.UI.Xaml.Controls.impl.h"

namespace wxl {

namespace xaml = winrt::Microsoft::UI::Xaml;
namespace foundation = winrt::Windows::Foundation;

namespace {

// Объект WinUI панели. Детей у него три: левая часть, правая часть и разделитель --
// последней, чтобы лежать поверх частей. Состояние перетаскивания живёт здесь
// же: обёртка wxl своего ничего не несёт.
struct SplitPanelObject : xaml::Controls::PanelT<SplitPanelObject> {
    SplitPanelObject() {
        grip_.Fill(xaml::Media::SolidColorBrush {winrt::Windows::UI::Color {0, 0, 0, 0}});
        grip_.as<xaml::IUIElementProtected>().ProtectedCursor(winrt::Microsoft::UI::Input::InputSystemCursor::Create(
            winrt::Microsoft::UI::Input::InputSystemCursorShape::SizeWestEast));
        grip_.PointerPressed({this, &SplitPanelObject::pressed});
        grip_.PointerMoved({this, &SplitPanelObject::moved});
        grip_.PointerReleased({this, &SplitPanelObject::released});
        grip_.PointerCaptureLost({this, &SplitPanelObject::lost});
        Children().Append(grip_);
    }

    // Имя для дерева элементов (Live Visual Tree): без него cppwinrt отвечает
    // именем первого интерфейса из implements.
    winrt::hstring GetRuntimeClassName() const { return L"wxl.SplitPanel"; }

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

    // Сколько левой части помещается в данную ширину: зазор и правая часть
    // остаются видны, пока ширины хватает на зазор.
    float fitted(float width) const {
        if (!std::isfinite(width)) {
            return length_;
        }
        return std::clamp(length_, 0.0f, std::max(0.0f, width - spacing_));
    }

    foundation::Size MeasureOverride(foundation::Size available) {
        float const length = fitted(available.Width);
        float const rest = std::isfinite(available.Width) ? std::max(0.0f, available.Width - length - spacing_)
                                                          : available.Width;
        foundation::Size desired {length + spacing_, 0};
        if (pane_) {
            pane_.Measure({length, available.Height});
            desired.Height = std::max(desired.Height, pane_.DesiredSize().Height);
        }
        if (content_) {
            content_.Measure({rest, available.Height});
            desired.Width += content_.DesiredSize().Width;
            desired.Height = std::max(desired.Height, content_.DesiredSize().Height);
        }
        grip_.Measure({spacing_, available.Height});
        return desired;
    }

    foundation::Size ArrangeOverride(foundation::Size final) {
        float const length = fitted(final.Width);
        float const rest = std::max(0.0f, final.Width - length - spacing_);
        if (pane_) {
            pane_.Arrange(foundation::Rect {0, 0, length, final.Height});
        }
        grip_.Arrange(foundation::Rect {length, 0, spacing_, final.Height});
        if (content_) {
            content_.Arrange(foundation::Rect {length + spacing_, 0, rest, final.Height});
        }
        return final;
    }

    static float minWidthOf(xaml::UIElement const& element) {
        auto const framework = element.try_as<xaml::FrameworkElement>();
        return framework ? static_cast<float>(framework.MinWidth()) : 0.0f;
    }

    // Граница не заходит за MinWidth ни одной из частей; если обе не
    // помещаются, левая берёт своё.
    float clamped(float length) const {
        float const least = pane_ ? minWidthOf(pane_) : 0.0f;
        float const most = ActualSize().x - spacing_ - (content_ ? minWidthOf(content_) : 0.0f);
        return std::max(least, std::min(length, most));
    }

    void pressed(foundation::IInspectable const&, xaml::Input::PointerRoutedEventArgs const& args) {
        if (grip_.CapturePointer(args.Pointer())) {
            dragging_ = true;
            from_ = args.GetCurrentPoint(*this).Position().X;
            start_ = fitted(ActualSize().x);
            args.Handled(true);
        }
    }

    void moved(foundation::IInspectable const&, xaml::Input::PointerRoutedEventArgs const& args) {
        if (!dragging_) {
            return;
        }
        float const length = clamped(start_ + args.GetCurrentPoint(*this).Position().X - from_);
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

    bool dragging_ = false;
    float from_ = 0;   // где нажали, в координатах панели
    float start_ = 0;  // ширина левой части в тот миг
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
