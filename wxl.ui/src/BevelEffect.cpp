// wxl::BevelEffect -- the composition behind it; see BevelEffect.h.
//
// The projection comes first, and with it every standard header it needs: the
// wxl headers below carry the wxl.core import.
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>

#include <algorithm>
#include <bit>
#include <string>
#include <utility>
#include <vector>

#include "BevelEffect.h"
#include "Object.impl.h"
#include <wxl/Microsoft.UI.Xaml.impl.h>
#include "impl/effect_helpers.h"

namespace wxl {

namespace composition = winrt::Microsoft::UI::Composition;
namespace xaml = winrt::Microsoft::UI::Xaml;

struct BevelEffect::Impl : core::sta_refcounted {
    Color light = rgba(255, 255, 255, 0.27);
    Color dark = rgba(0, 0, 0, 0.38);
    int colours = 0;
    // Rings written as relief edges, outermost first; empty, the rim is the
    // one ring of light and dark above.
    std::vector<std::pair<Color, Color>> rings;
    float thickness = 2.0f;
    float softness = 1.0f;
    float past = 0.0f;
    Thickness margin{};
    core::nullable<float> corner;
};

BevelEffect::BevelEffect() : impl_{new Impl, /*add_ref=*/false} {}
BevelEffect::BevelEffect(BevelEffect const& other) noexcept = default;
BevelEffect& BevelEffect::operator=(BevelEffect const& other) noexcept = default;
BevelEffect::~BevelEffect() = default;

void BevelEffect::setPositional(Color value) const {
    (impl_->colours++ == 0 ? impl_->light : impl_->dark) = value;
}

void BevelEffect::setPositional(Relief::Edges ring) const {
    impl_->rings.emplace_back(ring.nearLamp, ring.farFromLamp);
}

void BevelEffect::strokeThickness(double value) const { impl_->thickness = static_cast<float>(value); }
void BevelEffect::blurRadius(double value) const { impl_->softness = static_cast<float>(value); }
void BevelEffect::offset(double value) const { impl_->past = static_cast<float>(value); }
void BevelEffect::margin(Thickness value) const { impl_->margin = value; }
void BevelEffect::cornerRadius(CornerRadius value) const {
    impl_->corner = static_cast<float>(value.topLeft);
}

void BevelEffect::operator()(UIElement const& wrapper) const {
    auto const element = Object::Impl::as<xaml::UIElement>(wrapper);
    auto const host = xaml::Hosting::ElementCompositionPreview::GetElementVisual(element);
    auto const compositor = host.Compositor();

    float const left = static_cast<float>(impl_->margin.left);
    float const top = static_cast<float>(impl_->margin.top);
    float const right = static_cast<float>(impl_->margin.right);
    float const bottom = static_cast<float>(impl_->margin.bottom);

    // A negative margin puts the rim outside the element; the ShapeVisual clips
    // to its own bounds, so it grows by as much on that side.
    float const outLeft = std::max(0.0f, -left);
    float const outTop = std::max(0.0f, -top);
    float const outRight = std::max(0.0f, -right);
    float const outBottom = std::max(0.0f, -bottom);

    auto const rim = compositor.CreateShapeVisual();
    rim.RelativeSizeAdjustment({1.0f, 1.0f});
    rim.Size({outLeft + outRight, outTop + outBottom});
    rim.Offset({-outLeft, -outTop, 0.0f});

    // The rings share the stroke's width, the first one outermost, each a
    // stroke of its own along a path inset by the rings before it. One pair
    // of colours is one ring: the plain rim.
    std::vector<std::pair<Color, Color>> const rings =
        impl_->rings.empty() ? std::vector<std::pair<Color, Color>>{{impl_->light, impl_->dark}} : impl_->rings;
    float const width = impl_->thickness / static_cast<float>(rings.size());

    // Every ring's geometry, with how far its path lies inside the margin, so
    // the element's rounding can be handed to all of them once it is known.
    std::vector<std::pair<composition::CompositionRoundedRectangleGeometry, float>> geometries;

    for (std::size_t index = 0; index < rings.size(); ++index) {
        auto const& [light, dark] = rings[index];
        // Half the stroke, plus the rings outside this one.
        float const inset = width * (static_cast<float>(index) + 0.5f);

        // The path the stroke follows: the element less the margin, less the
        // inset on each side, so the stroke lies inside and not across the edge.
        std::wstring const w = L"(h.Size.X - k.X)";
        std::wstring const h = L"(h.Size.Y - k.Y)";
        std::wstring const span = L"Length(Vector2(" + w + L", " + h + L"))";
        auto const bind = [&](std::wstring const& formula) {
            auto const expression = compositor.CreateExpressionAnimation(formula);
            expression.SetReferenceParameter(L"h", host);
            expression.SetVector2Parameter(L"k", {left + right + 2 * inset, top + bottom + 2 * inset});
            expression.SetScalarParameter(L"a", impl_->past);
            expression.SetScalarParameter(L"s", impl_->softness);
            return expression;
        };

        auto const geometry = compositor.CreateRoundedRectangleGeometry();
        geometry.StartAnimation(L"Size", bind(L"Vector2(" + w + L", " + h + L")"));
        geometries.emplace_back(geometry, inset);

        // The change from light to dark runs through the middle of the box and
        // meets the shorter sides `a` pixels past the corners: on a wide box at
        // (w, a) and (0, h - a), on a tall one at (w - a, 0) and (a, h). The
        // axis is square to that line, and long enough to reach every corner.
        auto const brush = compositor.CreateLinearGradientBrush();
        brush.MappingMode(composition::CompositionMappingMode::Absolute);
        std::wstring const normal = L"Normalize(" + w + L" >= " + h + L" ? Vector2(" + h + L" - 2 * a, " + w
                                    + L") : Vector2(" + h + L", " + w + L" - 2 * a))";
        std::wstring const centre = L"Vector2(" + w + L" / 2, " + h + L" / 2)";
        brush.StartAnimation(L"StartPoint", bind(centre + L" - " + normal + L" * " + span + L" * 0.5"));
        brush.StartAnimation(L"EndPoint", bind(centre + L" + " + normal + L" * " + span + L" * 0.5"));

        // The softness in pixels, as a share of the axis.
        auto const stop = [&](float offset, Color color, wchar_t const* follows) {
            auto const at =
                compositor.CreateColorGradientStop(offset, std::bit_cast<winrt::Windows::UI::Color>(color));
            if (follows) at.StartAnimation(L"Offset", bind(follows));
            brush.ColorStops().Append(at);
        };
        std::wstring const before = L"Max(0, 0.5 - s / (2 * " + span + L"))";
        std::wstring const after = L"Min(1, 0.5 + s / (2 * " + span + L"))";
        stop(0.0f, light, nullptr);
        stop(0.5f, light, before.c_str());
        stop(0.5f, dark, after.c_str());
        stop(1.0f, dark, nullptr);

        auto const shape = compositor.CreateSpriteShape(geometry);
        shape.Offset({left + inset + outLeft, top + inset + outTop});
        shape.StrokeBrush(brush);
        shape.StrokeThickness(width);
        rim.Shapes().Append(shape);
    }

    // The element's rounding, less the margin and each ring's inset, is the
    // ring's own; a ring inside the rounding's reach gets square corners.
    auto const round = [geometries, less = std::max(left, top)](float radius) {
        for (auto const& [geometry, inset] : geometries) {
            float const r = std::max(0.0f, radius - less - inset);
            geometry.CornerRadius({r, r});
        }
    };
    if (impl_->corner) {
        round(*impl_->corner);
    } else if (auto const fe = element.try_as<xaml::FrameworkElement>()) {
        // The element's rounding is read once it is loaded: in its braces it
        // may be written after the effect.
        fe.Loaded([round](auto const& sender, auto&&) {
            round(impl::corner_of(sender.template as<xaml::UIElement>()));
        });
    }

    // Not before XAML has made the child it draws the element into: some
    // elements (a Border) clear these children when they make it, and would
    // throw the rim out with them. So the rim waits for that child frame by
    // frame, as HaloEffect does, and goes on top once it is there.
    auto const children = host.as<composition::ContainerVisual>().Children();
    if (children.Count() != 0) {
        children.InsertAtTop(rim);
        return;
    }
    auto const waiting = core::make_refcounted<winrt::event_token>();
    *waiting = xaml::Media::CompositionTarget::Rendering([children, rim, waiting](auto&&, auto&&) {
        if (children.Count() == 0) return;
        children.InsertAtTop(rim);
        xaml::Media::CompositionTarget::Rendering(*waiting);
    });
}

}  // namespace wxl
