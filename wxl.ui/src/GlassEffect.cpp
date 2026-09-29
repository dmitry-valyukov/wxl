// wxl::GlassEffect -- the two panes behind it; see GlassEffect.h.
//
// The projection comes first, and with it every standard header it needs: the
// wxl headers below carry the wxl.core import.
#include <winrt/Microsoft.Graphics.Canvas.Effects.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
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
#include "impl/effect_layer.h"
#include "impl/scene.h"

namespace wxl {

namespace composition = winrt::Microsoft::UI::Composition;
namespace canvas = winrt::Microsoft::Graphics::Canvas;
namespace effects = winrt::Microsoft::Graphics::Canvas::Effects;
namespace xaml = winrt::Microsoft::UI::Xaml;
namespace controls = winrt::Microsoft::UI::Xaml::Controls;
using winrt::Windows::Foundation::Numerics::float2;

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

// The corner the element rounds itself with, so the panes round the same.
float corner_of(xaml::FrameworkElement const& element) {
    if (auto const border = element.try_as<controls::Border>()) return static_cast<float>(border.CornerRadius().TopLeft);
    if (auto const control = element.try_as<controls::Control>()) return static_cast<float>(control.CornerRadius().TopLeft);
    return 0.0f;
}

// The glass: what lies behind the pane, blurred, and the tint laid over it.
// A DropShadow's blur radius is about three deviations of the Gaussian, so
// the same number gives the same reach here.
composition::CompositionBrush glass_brush(composition::Compositor const& compositor, Color color, float blurRadius) {
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
    return brush;
}

// A pane: a sprite clipped to a rounded rectangle, on whichever compositor.
struct Sheet {
    composition::SpriteVisual layer{nullptr};
    composition::CompositionRoundedRectangleGeometry shape{nullptr};

    void make(composition::Compositor const& compositor, Color color, float blurRadius, float opacity) {
        shape = compositor.CreateRoundedRectangleGeometry();
        layer = compositor.CreateSpriteVisual();
        layer.Brush(glass_brush(compositor, color, blurRadius));
        layer.Opacity(opacity);
        layer.Clip(compositor.CreateGeometricClip(shape));
    }

    void fit(float2 size, float radius) {
        if (!layer) return;
        layer.Size(size);
        shape.Size(size);
        shape.CornerRadius({radius, radius});
    }

    // The compositor copies what lies behind the layer into the effect when
    // the layer is drawn and keeps the copy: a neighbour under it that
    // changes later -- a picture decoded a frame after the pane went in --
    // is not noticed. Taking the layer out and putting it back is what makes
    // it copy again; a new brush on the same layer does not.
    void reattach(int z) {
        if (!layer) return;
        auto const parent = layer.Parent();
        if (!parent) return;
        parent.Children().Remove(layer);
        impl::insert_layer(parent.Children(), layer, z);
    }

    void take_off() {
        if (!layer) return;
        if (auto const parent = layer.Parent()) parent.Children().Remove(layer);
        layer = nullptr;
        shape = nullptr;
    }
};

// Where the island pane stands among the element's layers: under its pixels.
constexpr int paneZ = -1;

// One attached element's panes: made on Loaded, following the element on
// every layout, taken off on Unloaded. The element is held weakly -- its own
// events hold the handlers that hold this.
//
// Two panes, because what is behind the element lies in two trees. The
// window's picture is on the scene, which the island cannot see; a picture
// or a panel drawn by XAML is in the island, which the scene cannot see.
// The scene pane sits over the window's backdrop under the element's
// rectangle, the island pane sits among the element's own layers under
// its pixels, and where the island draws nothing it stays clear.
struct Pane : std::enable_shared_from_this<Pane> {
    Color color{};
    float blurRadius{};
    float opacity{};
    winrt::weak_ref<xaml::FrameworkElement> element;
    impl::scene scene;
    Sheet onScene;
    Sheet inIsland;
    winrt::event_token layoutUpdated{};

    void place(xaml::FrameworkElement const& fe) {
        auto const root = fe.XamlRoot();
        if (!root) return;

        if (auto const environment = root.ContentIslandEnvironment()) {
            scene = impl::scene_of(environment.AppWindowId());
        }
        if (scene) {
            onScene.make(scene.compositor, color, blurRadius, opacity);
            scene.root.Children().InsertAtTop(onScene.layer);
        }

        auto const host = xaml::Hosting::ElementCompositionPreview::GetElementVisual(fe);
        impl::when_drawn(host.as<composition::ContainerVisual>(),
                         [weak = weak_from_this(), host](auto const& children) {
                             auto const self = weak.lock();
                             if (!self || self->inIsland.layer) return;
                             self->inIsland.make(host.Compositor(), self->color, self->blurRadius, self->opacity);
                             if (auto const fe = self->element.get()) self->follow(fe);
                             impl::insert_layer(children, self->inIsland.layer, paneZ);
                         });

        follow(fe);
        layoutUpdated = fe.LayoutUpdated([weak = weak_from_this()](auto&&, auto&&) {
            auto const self = weak.lock();
            if (!self) return;
            if (auto const fe = self->element.get()) self->follow(fe);
        });
    }

    // The element's rectangle: in the island, its own size in logical
    // pixels; on the scene, its place in the window in physical ones -- the
    // island covers the client area from its corner, and its rasterization
    // scale is the DPI with the window's zoom. Whatever appears or grows
    // behind the element comes with a layout pass, so the island pane is
    // reattached here to copy the background afresh.
    void follow(xaml::FrameworkElement const& fe) {
        auto const root = fe.XamlRoot();
        if (!root) return;
        float2 const size{static_cast<float>(fe.ActualWidth()), static_cast<float>(fe.ActualHeight())};
        float const radius = corner_of(fe);
        inIsland.fit(size, radius);
        inIsland.reattach(paneZ);

        if (!onScene.layer) return;
        float const scale = static_cast<float>(root.RasterizationScale());
        auto const corner = fe.TransformToVisual(nullptr).TransformPoint({0.0f, 0.0f});
        onScene.layer.Offset({corner.X * scale, corner.Y * scale, 0.0f});
        onScene.fit(size * scale, radius * scale);
    }

    void remove(xaml::FrameworkElement const& fe) {
        fe.LayoutUpdated(layoutUpdated);
        onScene.take_off();
        inIsland.take_off();
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
