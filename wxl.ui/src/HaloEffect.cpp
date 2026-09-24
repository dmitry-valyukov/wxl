// wxl::HaloEffect -- the composition behind it; see HaloEffect.h.
//
// The projection comes first, and with it every standard header it needs: the
// wxl headers below carry the wxl.core import.
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Numerics.h>

#include <bit>
#include <functional>
#include <utility>

#include "HaloEffect.h"
#include "Object.impl.h"
#include "generated/Microsoft.UI.Composition.impl.h"
#include "generated/Microsoft.UI.Xaml.impl.h"
#include "impl/effect_layer.h"

namespace wxl {

namespace composition = winrt::Microsoft::UI::Composition;
namespace xaml = winrt::Microsoft::UI::Xaml;

struct HaloEffect::State : core::sta_refcounted {
    Color color = rgb(0, 0, 0);
    float blurRadius = 9.0f;
    float opacity = 1.0f;
    Vector3 offset{};
    int32_t z = -1;
};

HaloEffect::HaloEffect() : state_{new State, /*add_ref=*/false} {}
HaloEffect::HaloEffect(HaloEffect const& other) noexcept = default;
HaloEffect& HaloEffect::operator=(HaloEffect const& other) noexcept = default;
HaloEffect::~HaloEffect() = default;

void HaloEffect::color(Color value) const { state_->color = value; }
void HaloEffect::blurRadius(double value) const { state_->blurRadius = static_cast<float>(value); }
void HaloEffect::opacity(double value) const { state_->opacity = static_cast<float>(value); }
void HaloEffect::offset(Vector3 value) const { state_->offset = value; }
void HaloEffect::zIndex(int32_t value) const { state_->z = value; }

void HaloEffect::wear(UIElement const& wrapper, std::function<CompositionBrush()> maskOf) const {
    core::intrusive_ptr<State> const s = state_;
    auto const element = Object::Impl::as<xaml::UIElement>(wrapper);
    auto const host = xaml::Hosting::ElementCompositionPreview::GetElementVisual(element);

    // The layer is made once the element is drawn, from the compositor the
    // element's own visual was made by, and the mask is taken then too.
    impl::when_drawn(host.as<composition::ContainerVisual>(), [host, maskOf = std::move(maskOf), s](auto const& children) {
        auto const compositor = host.Compositor();
        auto const mask = Object::Impl::as<composition::CompositionBrush>(maskOf());

        auto const glow = compositor.CreateDropShadow();
        glow.Color(std::bit_cast<winrt::Windows::UI::Color>(s->color));
        glow.BlurRadius(s->blurRadius);
        glow.Opacity(s->opacity);
        glow.Offset({s->offset.x, s->offset.y, s->offset.z});
        glow.Mask(mask);

        auto const halo = compositor.CreateSpriteVisual();
        halo.Shadow(glow);

        // No size of its own: the halo's parent is the element's own visual,
        // XAML keeps that at the element's layout size, and a relative
        // adjustment of one says "the same".
        halo.RelativeSizeAdjustment({1.0f, 1.0f});

        impl::insert_layer(children, halo, s->z);
    });
}

}  // namespace wxl
