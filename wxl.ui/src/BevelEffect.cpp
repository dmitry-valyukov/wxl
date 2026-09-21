// wxl::BevelEffect -- the composition behind it; see BevelEffect.h.
//
// The projection comes first, and with it every standard header it needs: the
// wxl headers below carry the wxl.core import.
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>

#include <algorithm>
#include <bit>
#include <memory>
#include <optional>
#include <string>

#include "BevelEffect.h"
#include "Object.impl.h"
#include "generated/Microsoft.UI.Xaml.impl.h"

namespace wxl {

namespace composition = winrt::Microsoft::UI::Composition;
namespace xaml = winrt::Microsoft::UI::Xaml;

struct BevelEffect::State : core::sta_refcounted {
    Color light = rgba(255, 255, 255, 0.27);
    Color dark = rgba(0, 0, 0, 0.38);
    int colours = 0;
    float thickness = 2.0f;
    float softness = 1.0f;
    float past = 0.0f;
    Thickness margin{};
    std::optional<float> corner;
};

BevelEffect::BevelEffect() : state_{new State, /*add_ref=*/false} {}
BevelEffect::BevelEffect(BevelEffect const& other) noexcept = default;
BevelEffect& BevelEffect::operator=(BevelEffect const& other) noexcept = default;
BevelEffect::~BevelEffect() = default;

void BevelEffect::setPositional(Color value) const {
    (state_->colours++ == 0 ? state_->light : state_->dark) = value;
}

void BevelEffect::strokeThickness(double value) const { state_->thickness = static_cast<float>(value); }
void BevelEffect::blurRadius(double value) const { state_->softness = static_cast<float>(value); }
void BevelEffect::offset(double value) const { state_->past = static_cast<float>(value); }
void BevelEffect::margin(Thickness value) const { state_->margin = value; }
void BevelEffect::cornerRadius(CornerRadius value) const {
    state_->corner = static_cast<float>(value.topLeft);
}

namespace {

// The element's own rounding, for an element that has one.
std::optional<float> own_corner(xaml::UIElement const& element) {
    if (auto const border = element.try_as<xaml::Controls::Border>()) {
        return static_cast<float>(border.CornerRadius().TopLeft);
    }
    if (auto const control = element.try_as<xaml::Controls::Control>()) {
        return static_cast<float>(control.CornerRadius().TopLeft);
    }
    if (auto const grid = element.try_as<xaml::Controls::Grid>()) {
        return static_cast<float>(grid.CornerRadius().TopLeft);
    }
    if (auto const stack = element.try_as<xaml::Controls::StackPanel>()) {
        return static_cast<float>(stack.CornerRadius().TopLeft);
    }
    if (auto const relative = element.try_as<xaml::Controls::RelativePanel>()) {
        return static_cast<float>(relative.CornerRadius().TopLeft);
    }
    return std::nullopt;
}

}  // namespace

void BevelEffect::operator()(UIElement const& wrapper) const {
    State const& s = *state_;
    auto const element = Object::Impl::as<xaml::UIElement>(wrapper);
    auto const host = xaml::Hosting::ElementCompositionPreview::GetElementVisual(element);
    auto const compositor = host.Compositor();

    float const left = static_cast<float>(s.margin.left);
    float const top = static_cast<float>(s.margin.top);
    float const right = static_cast<float>(s.margin.right);
    float const bottom = static_cast<float>(s.margin.bottom);
    float const inset = s.thickness * 0.5f;

    // The path the stroke follows: the element less the margin, less half the
    // stroke on each side, so the stroke lies inside and not across the edge.
    std::wstring const w = L"(h.Size.X - k.X)";
    std::wstring const h = L"(h.Size.Y - k.Y)";
    std::wstring const span = L"Length(Vector2(" + w + L", " + h + L"))";
    auto const bind = [&](std::wstring const& formula) {
        auto const expression = compositor.CreateExpressionAnimation(formula);
        expression.SetReferenceParameter(L"h", host);
        expression.SetVector2Parameter(
            L"k", {left + right + s.thickness, top + bottom + s.thickness});
        expression.SetScalarParameter(L"a", s.past);
        expression.SetScalarParameter(L"s", s.softness);
        return expression;
    };

    auto const geometry = compositor.CreateRoundedRectangleGeometry();
    geometry.StartAnimation(L"Size", bind(L"Vector2(" + w + L", " + h + L")"));

    auto const round = [geometry, inset, less = std::max(left, top)](float radius) {
        float const r = std::max(0.0f, radius - less - inset);
        geometry.CornerRadius({r, r});
    };
    if (s.corner) {
        round(*s.corner);
    } else if (auto const fe = element.try_as<xaml::FrameworkElement>()) {
        // The element's rounding is read once it is loaded: in its braces it
        // may be written after the effect.
        fe.Loaded([round](auto const& sender, auto&&) {
            if (auto const r = own_corner(sender.template as<xaml::UIElement>())) round(*r);
        });
    }

    // The change from light to dark runs through the middle of the box and
    // meets the shorter sides `a` pixels past the corners: on a wide box at
    // (w, a) and (0, h - a), on a tall one at (w - a, 0) and (a, h). The axis
    // is square to that line, and long enough to reach every corner.
    auto const brush = compositor.CreateLinearGradientBrush();
    brush.MappingMode(composition::CompositionMappingMode::Absolute);
    std::wstring const normal = L"Normalize(" + w + L" >= " + h + L" ? Vector2(" + h + L" - 2 * a, " + w
                                + L") : Vector2(" + h + L", " + w + L" - 2 * a))";
    std::wstring const centre = L"Vector2(" + w + L" / 2, " + h + L" / 2)";
    brush.StartAnimation(L"StartPoint", bind(centre + L" - " + normal + L" * " + span + L" * 0.5"));
    brush.StartAnimation(L"EndPoint", bind(centre + L" + " + normal + L" * " + span + L" * 0.5"));

    // The softness in pixels, as a share of the axis.
    auto const stop = [&](float offset, Color color, wchar_t const* follows) {
        auto const at = compositor.CreateColorGradientStop(offset, std::bit_cast<winrt::Windows::UI::Color>(color));
        if (follows) at.StartAnimation(L"Offset", bind(follows));
        brush.ColorStops().Append(at);
    };
    std::wstring const before = L"Max(0, 0.5 - s / (2 * " + span + L"))";
    std::wstring const after = L"Min(1, 0.5 + s / (2 * " + span + L"))";
    stop(0.0f, s.light, nullptr);
    stop(0.5f, s.light, before.c_str());
    stop(0.5f, s.dark, after.c_str());
    stop(1.0f, s.dark, nullptr);

    // A negative margin puts the rim outside the element; the ShapeVisual clips
    // to its own bounds, so it grows by as much on that side.
    float const outLeft = std::max(0.0f, -left);
    float const outTop = std::max(0.0f, -top);
    float const outRight = std::max(0.0f, -right);
    float const outBottom = std::max(0.0f, -bottom);

    auto const shape = compositor.CreateSpriteShape(geometry);
    shape.Offset({left + inset + outLeft, top + inset + outTop});
    shape.StrokeBrush(brush);
    shape.StrokeThickness(s.thickness);

    auto const rim = compositor.CreateShapeVisual();
    rim.RelativeSizeAdjustment({1.0f, 1.0f});
    rim.Size({outLeft + outRight, outTop + outBottom});
    rim.Offset({-outLeft, -outTop, 0.0f});
    rim.Shapes().Append(shape);

    // Not before XAML has made the child it draws the element into: some
    // elements (a Border) clear these children when they make it, and would
    // throw the rim out with them. So the rim waits for that child frame by
    // frame, as HaloEffect does, and goes on top once it is there.
    auto const children = host.as<composition::ContainerVisual>().Children();
    if (children.Count() != 0) {
        children.InsertAtTop(rim);
        return;
    }
    auto const waiting = std::make_shared<winrt::event_token>();
    *waiting = xaml::Media::CompositionTarget::Rendering([children, rim, waiting](auto&&, auto&&) {
        if (children.Count() == 0) return;
        children.InsertAtTop(rim);
        xaml::Media::CompositionTarget::Rendering(*waiting);
    });
}

}  // namespace wxl
