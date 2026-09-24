// wxl::GaussianBlurEffect -- the effect graph behind it; see
// GaussianBlurEffect.h.
//
// The projection comes first, and with it every standard header it needs: the
// wxl headers below carry the wxl.core import.
#include <winrt/Microsoft.Graphics.Canvas.Effects.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Graphics.Effects.h>

#include <algorithm>
#include <bit>
#include <functional>
#include <utility>

#include "GaussianBlurEffect.h"
#include "Object.impl.h"
#include "generated/Microsoft.UI.Composition.impl.h"
#include "generated/Microsoft.UI.Xaml.impl.h"
#include "impl/effect_layer.h"

namespace wxl {

namespace composition = winrt::Microsoft::UI::Composition;
namespace canvas = winrt::Microsoft::Graphics::Canvas;
namespace effects = winrt::Microsoft::Graphics::Canvas::Effects;
namespace xaml = winrt::Microsoft::UI::Xaml;

struct GaussianBlurEffect::State : core::sta_refcounted {
    Color color = rgb(0, 0, 0);
    float blurRadius = 9.0f;
    float opacity = 1.0f;
    float gamma = 1.0f;
    int32_t z = -1;
};

GaussianBlurEffect::GaussianBlurEffect() : state_{new State, /*add_ref=*/false} {}
GaussianBlurEffect::GaussianBlurEffect(GaussianBlurEffect const& other) noexcept = default;
GaussianBlurEffect& GaussianBlurEffect::operator=(GaussianBlurEffect const& other) noexcept = default;
GaussianBlurEffect::~GaussianBlurEffect() = default;

void GaussianBlurEffect::color(Color value) const { state_->color = value; }
void GaussianBlurEffect::blurRadius(double value) const { state_->blurRadius = static_cast<float>(value); }
void GaussianBlurEffect::opacity(double value) const {
    state_->opacity = static_cast<float>(std::clamp(value, 0.0, 1.0));
}
void GaussianBlurEffect::gamma(double value) const { state_->gamma = static_cast<float>(std::max(value, 0.01)); }
void GaussianBlurEffect::zIndex(int32_t value) const { state_->z = value; }

void GaussianBlurEffect::wear(UIElement const& wrapper, std::function<CompositionBrush()> maskOf) const {
    core::intrusive_ptr<State> const s = state_;
    auto const element = Object::Impl::as<xaml::UIElement>(wrapper);
    auto const host = xaml::Hosting::ElementCompositionPreview::GetElementVisual(element);

    // The layer is made once the element is drawn, and the mask is taken
    // then too: before that there are no glyphs to take the alpha of.
    impl::when_drawn(host.as<composition::ContainerVisual>(), [host, maskOf = std::move(maskOf), s](auto const& children) {
        auto const compositor = host.Compositor();
        auto const mask = Object::Impl::as<composition::CompositionBrush>(maskOf());

        // The graph: the glyphs' alpha, blurred; its alpha raised to gamma;
        // the colour kept where that alpha is. A DropShadow's blur radius is
        // about three deviations of the Gaussian, so the same number gives
        // the same reach here.
        effects::GaussianBlurEffect blur;
        blur.Source(composition::CompositionEffectSourceParameter{L"mask"});
        blur.BlurAmount(s->blurRadius / 3.0f);
        blur.BorderMode(effects::EffectBorderMode::Soft);

        effects::GammaTransferEffect bend;
        bend.Source(blur);
        bend.AlphaExponent(s->gamma);
        bend.RedDisable(true);
        bend.GreenDisable(true);
        bend.BlueDisable(true);

        effects::ColorSourceEffect fill;
        Color tint = s->color;
        tint.A = static_cast<uint8_t>(tint.A * s->opacity + 0.5f);
        fill.Color(std::bit_cast<winrt::Windows::UI::Color>(tint));

        effects::CompositeEffect glow;
        glow.Mode(canvas::CanvasComposite::DestinationIn);
        glow.Sources().Append(fill);
        glow.Sources().Append(bend);

        auto const brush = compositor.CreateEffectFactory(glow).CreateBrush();
        brush.SetSourceParameter(L"mask", mask);

        // The glow reaches past the glyphs by the blur, so the layer is the
        // element's size plus that margin all round, and the mask is drawn
        // at its own size in the middle of it.
        float const margin = s->blurRadius;
        if (auto const surface = mask.try_as<composition::CompositionSurfaceBrush>()) {
            surface.Stretch(composition::CompositionStretch::None);
            surface.HorizontalAlignmentRatio(0.5f);
            surface.VerticalAlignmentRatio(0.5f);
        }

        auto const layer = compositor.CreateSpriteVisual();
        layer.Brush(brush);
        layer.RelativeSizeAdjustment({1.0f, 1.0f});
        layer.Size({2.0f * margin, 2.0f * margin});
        layer.Offset({-margin, -margin, 0.0f});

        impl::insert_layer(children, layer, s->z);
    });
}

}  // namespace wxl
