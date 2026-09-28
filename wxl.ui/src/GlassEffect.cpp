// wxl::GlassEffect -- the pane on the scene behind it; see GlassEffect.h.
//
// The projection comes first, and with it every standard header it needs: the
// wxl headers below carry the wxl.core import.
#include <winrt/Microsoft.Graphics.Canvas.Effects.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Graphics.Effects.h>

#include <algorithm>
#include <bit>
#include <memory>

#include "GlassEffect.h"
#include "Object.impl.h"
#include "generated/Microsoft.UI.Xaml.impl.h"
#include "impl/scene.h"

namespace wxl {

namespace composition = winrt::Microsoft::UI::Composition;
namespace canvas = winrt::Microsoft::Graphics::Canvas;
namespace effects = winrt::Microsoft::Graphics::Canvas::Effects;
namespace xaml = winrt::Microsoft::UI::Xaml;
namespace controls = winrt::Microsoft::UI::Xaml::Controls;

struct GlassEffect::State : core::sta_refcounted {
    Color color = rgba(255, 255, 255, 0.0);
    float blurRadius = 24.0f;
    float opacity = 1.0f;
};

GlassEffect::GlassEffect() : state_{new State, /*add_ref=*/false} {}
GlassEffect::GlassEffect(GlassEffect const& other) noexcept = default;
GlassEffect& GlassEffect::operator=(GlassEffect const& other) noexcept = default;
GlassEffect::~GlassEffect() = default;

void GlassEffect::color(Color value) const { state_->color = value; }
void GlassEffect::blurRadius(double value) const { state_->blurRadius = static_cast<float>(std::max(value, 0.0)); }
void GlassEffect::opacity(double value) const {
    state_->opacity = static_cast<float>(std::clamp(value, 0.0, 1.0));
}

namespace {

// The corner the element rounds itself with, so the pane rounds the same.
float corner_of(xaml::FrameworkElement const& element) {
    if (auto const border = element.try_as<controls::Border>()) return static_cast<float>(border.CornerRadius().TopLeft);
    if (auto const control = element.try_as<controls::Control>()) return static_cast<float>(control.CornerRadius().TopLeft);
    return 0.0f;
}

// One attached element's pane: made on Loaded, following the element on every
// layout, taken off the scene on Unloaded. The element is held weakly -- its
// own events hold the handlers that hold this.
struct Pane : std::enable_shared_from_this<Pane> {
    Color color{};
    float blurRadius{};
    float opacity{};
    winrt::weak_ref<xaml::FrameworkElement> element;
    impl::scene scene;
    composition::SpriteVisual layer{nullptr};
    composition::CompositionRoundedRectangleGeometry shape{nullptr};
    winrt::event_token layoutUpdated{};

    void place(xaml::FrameworkElement const& fe) {
        auto const root = fe.XamlRoot();
        if (!root) return;
        auto const environment = root.ContentIslandEnvironment();
        if (!environment) return;
        scene = impl::scene_of(environment.AppWindowId());
        if (!scene) return;

        // The graph: what lies behind the pane, blurred, and the tint laid
        // over it. A DropShadow's blur radius is about three deviations of
        // the Gaussian, so the same number gives the same reach here.
        auto const compositor = scene.compositor;
        effects::GaussianBlurEffect blur;
        blur.Source(composition::CompositionEffectSourceParameter{L"behind"});
        blur.BlurAmount(blurRadius / 3.0f);
        blur.BorderMode(effects::EffectBorderMode::Hard);

        winrt::Windows::Graphics::Effects::IGraphicsEffect graph = blur;
        if (color.A != 0) {
            effects::ColorSourceEffect tint;
            tint.Color(std::bit_cast<winrt::Windows::UI::Color>(color));
            effects::CompositeEffect tinted;
            tinted.Mode(canvas::CanvasComposite::SourceOver);
            tinted.Sources().Append(blur);
            tinted.Sources().Append(tint);
            graph = tinted;
        }

        auto const brush = compositor.CreateEffectFactory(graph).CreateBrush();
        brush.SetSourceParameter(L"behind", compositor.CreateBackdropBrush());

        shape = compositor.CreateRoundedRectangleGeometry();
        layer = compositor.CreateSpriteVisual();
        layer.Brush(brush);
        layer.Opacity(opacity);
        layer.Clip(compositor.CreateGeometricClip(shape));
        scene.root.Children().InsertAtTop(layer);

        follow(fe);
        layoutUpdated = fe.LayoutUpdated([weak = weak_from_this()](auto&&, auto&&) {
            auto const self = weak.lock();
            if (!self) return;
            if (auto const fe = self->element.get()) self->follow(fe);
        });
    }

    // The element's rectangle in the window, in the scene's physical pixels:
    // the island covers the client area from its corner, and its
    // rasterization scale is the DPI with the window's zoom.
    void follow(xaml::FrameworkElement const& fe) {
        auto const root = fe.XamlRoot();
        if (!root || !layer) return;
        float const scale = static_cast<float>(root.RasterizationScale());
        auto const corner = fe.TransformToVisual(nullptr).TransformPoint({0.0f, 0.0f});
        winrt::Windows::Foundation::Numerics::float2 const size{
            static_cast<float>(fe.ActualWidth()) * scale, static_cast<float>(fe.ActualHeight()) * scale};
        float const radius = corner_of(fe) * scale;

        layer.Offset({corner.X * scale, corner.Y * scale, 0.0f});
        layer.Size(size);
        shape.Size(size);
        shape.CornerRadius({radius, radius});
    }

    void remove(xaml::FrameworkElement const& fe) {
        if (!layer) return;
        fe.LayoutUpdated(layoutUpdated);
        scene.root.Children().Remove(layer);
        layer = nullptr;
        shape = nullptr;
        scene = {};
    }
};

}  // namespace

void GlassEffect::attach(FrameworkElement const& wrapper) const {
    auto const element = Object::Impl::as<xaml::FrameworkElement>(wrapper);
    auto const pane = std::make_shared<Pane>();
    pane->color = state_->color;
    pane->blurRadius = state_->blurRadius;
    pane->opacity = state_->opacity;
    pane->element = element;

    element.Loaded([pane](auto const& sender, auto&&) {
        pane->place(sender.template as<xaml::FrameworkElement>());
    });
    element.Unloaded([pane](auto const& sender, auto&&) {
        pane->remove(sender.template as<xaml::FrameworkElement>());
    });
    if (element.IsLoaded()) pane->place(element);
}

}  // namespace wxl
