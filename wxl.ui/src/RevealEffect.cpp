// wxl::RevealEffect -- the lamps and the layers behind it; see RevealEffect.h.
//
// The projection and the Windows headers come first, and with them every
// standard header they need: the wxl headers below carry the wxl.core import.
#include "platform.h"

#include <winrt/Microsoft.Graphics.Canvas.Effects.h>
#include <winrt/Microsoft.UI.Composition.Effects.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Graphics.Effects.h>

#include <d2d1_1.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <vector>

#include "RevealEffect.h"
#include "DrawingSurface.h"
#include "Object.impl.h"
#include <wxl/Microsoft.UI.Composition.impl.h>
#include <wxl/Microsoft.UI.Xaml.impl.h>
#include "impl/effect_layer.h"
#include "impl/pointer_lamp.h"

namespace wxl {

namespace composition = winrt::Microsoft::UI::Composition;
namespace canvas = winrt::Microsoft::Graphics::Canvas;
namespace effects = winrt::Microsoft::Graphics::Canvas::Effects;
namespace xaml = winrt::Microsoft::UI::Xaml;

namespace {

// An edge is a thin line and takes twice the light a face does to show as
// much.
constexpr float faceGain = 1.0f;
constexpr float edgeGain = 2.0f;

// The name the lighting goes by inside both effect graphs, and its one
// property a brush can change after it is made.
constexpr wchar_t const* diffuseProperty = L"Light.DiffuseAmount";

// The mask of an edge is drawn twice as dense as layout units, so it stays
// sharp up to a scale of 200 %.
constexpr float ringScale = 2.0f;

// The light that falls on a layer, as a picture: its colour, and its
// brightness for the alpha -- so the layer shows where the lamp reaches and
// is clear where it does not. The lighting has no source of its own; what it
// shades is the flat surface of the sprite that wears the brush.
effects::ColorMatrixEffect glow(float diffuse, float gain) {
    composition::Effects::SceneLightingEffect light;
    light.Name(L"Light");
    light.AmbientAmount(0.0f);
    light.DiffuseAmount(diffuse);
    light.SpecularAmount(0.0f);

    effects::Matrix5x4 matrix{};
    matrix.M11 = gain;
    matrix.M22 = gain;
    matrix.M33 = gain;
    matrix.M14 = gain * 0.2125f;
    matrix.M24 = gain * 0.7154f;
    matrix.M34 = gain * 0.0721f;

    effects::ColorMatrixEffect toAlpha;
    toAlpha.Source(light);
    toAlpha.ColorMatrix(matrix);
    toAlpha.AlphaMode(canvas::CanvasAlphaMode::Straight);
    return toAlpha;
}

using impl::Beam;
using impl::Lamp;

// The narrow light: the lamp, and the one brush every face it lights wears.
struct FaceLight : Beam {
    composition::CompositionEffectBrush brush{nullptr};

    FaceLight() : Beam{140.0f, 0.69f} {}

    composition::CompositionBrush face(composition::Compositor const& compositor) {
        if (!brush) {
            brush = compositor.CreateEffectFactory(glow(diffuse, faceGain), {diffuseProperty}).CreateBrush();
        }
        return brush;
    }

    void set_diffuse(double value) {
        diffuse = static_cast<float>(std::max(value, 0.0));
        if (brush) brush.Properties().InsertScalar(diffuseProperty, diffuse);
    }
};

// The mask of an edge: a ring of one rounding, drawn small and stretched by
// its nine parts to whatever element wears it, and the brush that cuts the
// light with it. Elements of the same rounding share one.
struct Ring {
    float radius;
    DrawingSurface surface;
    composition::CompositionNineGridBrush frame;
    composition::CompositionEffectBrush brush;

    void draw(float thickness) {
        // The corners and the sides keep their size, by the rounding and the
        // stroke; two pixels of middle are all that stretches.
        int const corner = static_cast<int>(std::ceil((radius + thickness) * ringScale));
        int const side = 2 * corner + 2;
        if (surface.size().width != side) surface.resize({side, side});

        surface.draw([&](ID2D1DeviceContext* context) {
            winrt::com_ptr<ID2D1SolidColorBrush> white;
            winrt::check_hresult(context->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), white.put()));
            // A stroke lies on both sides of its line: the line goes half the
            // stroke inside the outline.
            float const half = thickness * ringScale / 2.0f;
            float const turn = std::max(radius * ringScale - half, 0.0f);
            float const end = static_cast<float>(side) - half;
            context->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
            context->DrawRoundedRectangle(D2D1_ROUNDED_RECT{D2D1::RectF(half, half, end, end), turn, turn},
                                          white.get(), thickness * ringScale);
        });
        frame.SetInsets(static_cast<float>(corner));
    }
};

// One lit element, as the wide light keeps it: its edge layer, the outline
// its face is clipped to, the element's own rounding and the one it wears.
struct Edge {
    composition::SpriteVisual layer;
    composition::CompositionRoundedRectangleGeometry outline;
    float own;
    float radius;
};

// The wide light: the lamp, the edges it lights and the rings they wear.
struct EdgeLight : Beam {
    float thickness = 1.5f;
    core::nullable<float> corner;
    composition::CompositionEffectFactory factory{nullptr};
    std::vector<Ring> rings;
    std::vector<Edge> edges;

    EdgeLight() : Beam{340.0f, 1.24f} {}

    Ring& ring_of(composition::Compositor const& compositor, float radius) {
        for (Ring& ring : rings) {
            if (ring.radius == radius) return ring;
        }
        // A ring no edge wears any more is drawn anew rather than replaced.
        for (Ring& ring : rings) {
            bool const worn = std::ranges::any_of(edges, [&](Edge const& edge) { return edge.radius == ring.radius; });
            if (worn) continue;
            ring.radius = radius;
            ring.draw(thickness);
            return ring;
        }

        if (!factory) {
            // The light of the edge, cut by the ring: of the lower input,
            // what is under the alpha of the upper one stays.
            effects::CompositeEffect cut;
            cut.Mode(canvas::CanvasComposite::DestinationIn);
            cut.Sources().Append(glow(diffuse, edgeGain));
            cut.Sources().Append(composition::CompositionEffectSourceParameter{L"Ring"});
            factory = compositor.CreateEffectFactory(cut, {diffuseProperty});
        }

        DrawingSurface const surface{Object::Impl::wrap<Compositor>(composition::Compositor{compositor}),
                                     SizeInt32{0, 0}};
        auto const mask = Object::Impl::as<composition::CompositionSurfaceBrush>(surface.brush());
        mask.Stretch(composition::CompositionStretch::Fill);

        auto const frame = compositor.CreateNineGridBrush();
        frame.Source(mask);
        frame.SetInsetScales(1.0f / ringScale);

        auto const brush = factory.CreateBrush();
        brush.SetSourceParameter(L"Ring", frame);
        brush.Properties().InsertScalar(diffuseProperty, diffuse);

        rings.push_back(Ring{radius, surface, frame, brush});
        rings.back().draw(thickness);
        return rings.back();
    }

    void dress(Edge const& edge) {
        edge.outline.CornerRadius({edge.radius, edge.radius});
        edge.layer.Brush(ring_of(edge.layer.Compositor(), edge.radius).brush);
    }

    void add(composition::SpriteVisual const& layer, composition::CompositionRoundedRectangleGeometry const& outline,
             float own) {
        edges.push_back(Edge{layer, outline, own, corner ? *corner : own});
        dress(edges.back());
    }

    void drop(composition::SpriteVisual const& layer) {
        std::erase_if(edges, [&](Edge const& edge) { return edge.layer == layer; });
    }

    void set_diffuse(double value) {
        diffuse = static_cast<float>(std::max(value, 0.0));
        for (Ring const& ring : rings) ring.brush.Properties().InsertScalar(diffuseProperty, diffuse);
    }

    void set_thickness(double value) {
        thickness = static_cast<float>(std::max(value, 0.0));
        for (Ring& ring : rings) ring.draw(thickness);
    }

    // Every edge has its new rounding before the first ring is looked for,
    // so a ring left without edges is seen to be free.
    void set_corner(float value) {
        corner = value;
        for (Edge& edge : edges) edge.radius = value;
        for (Edge const& edge : edges) dress(edge);
    }
};

// One attached element's two layers: made on Loaded, taken off on Unloaded.
// The element's own events hold the handlers that hold this.
struct Patch : std::enable_shared_from_this<Patch> {
    core::intrusive_ptr<FaceLight> hover;
    core::intrusive_ptr<EdgeLight> border;
    int z{};
    winrt::weak_ref<xaml::UIElement> root;
    composition::SpriteVisual face{nullptr};
    composition::SpriteVisual edge{nullptr};

    void place(xaml::FrameworkElement const& fe) {
        if (face) return;
        auto const xamlRoot = fe.XamlRoot();
        if (!xamlRoot) return;
        auto const island = xamlRoot.Content();
        if (!island) return;
        root = island;

        auto const host = xaml::Hosting::ElementCompositionPreview::GetElementVisual(fe);
        auto const compositor = host.Compositor();

        // The face is rounded as the edge is, and the outline keeps the
        // element's size by an expression over the visual XAML sizes itself.
        auto const outline = compositor.CreateRoundedRectangleGeometry();
        auto const follow = compositor.CreateExpressionAnimation(L"host.Size");
        follow.SetReferenceParameter(L"host", host);
        outline.StartAnimation(L"Size", follow);

        face = compositor.CreateSpriteVisual();
        face.RelativeSizeAdjustment({1.0f, 1.0f});
        face.Clip(compositor.CreateGeometricClip(outline));
        face.Brush(hover->face(compositor));

        edge = compositor.CreateSpriteVisual();
        edge.RelativeSizeAdjustment({1.0f, 1.0f});
        border->add(edge, outline, impl::corner_of(fe));

        // Aimed at before they are shown: see Lamp::shining.
        hover->light_up(island, face);
        border->light_up(island, edge);

        impl::when_drawn(host.as<composition::ContainerVisual>(),
                         [weak = weak_from_this(), layer = face](auto const& children) {
                             auto const self = weak.lock();
                             // Unloaded before the frame came: nothing to put there.
                             if (!self || self->face != layer) return;
                             impl::insert_layer(children, self->face, self->z);
                             impl::insert_layer(children, self->edge, self->z);
                         });
    }

    void remove() {
        if (!face) return;
        for (auto const& layer : {face, edge}) {
            if (auto const parent = layer.Parent()) parent.Children().Remove(layer);
        }
        if (auto const island = root.get()) {
            hover->leave(island, face);
            border->leave(island, edge);
        }
        border->drop(edge);
        face = nullptr;
        edge = nullptr;
    }

    void press() {
        if (auto const island = root.get()) hover->flash(island);
    }
};

}  // namespace

struct HoverLight::State final : FaceLight {};

impl::Beam& HoverLight::beam() const { return *state_; }

HoverLight::HoverLight() : state_{new State, /*add_ref=*/false} {}
HoverLight::HoverLight(HoverLight const& other) noexcept = default;
HoverLight& HoverLight::operator=(HoverLight const& other) noexcept = default;
HoverLight::~HoverLight() = default;

void HoverLight::size(double value) const { state_->set_size(value); }
void HoverLight::fullSize(double value) const { state_->set_full(value); }
void HoverLight::height(double value) const { state_->set_height(value); }
void HoverLight::intensity(double value) const { state_->set_intensity(value); }
void HoverLight::diffuseAmount(double value) const { state_->set_diffuse(value); }
void HoverLight::color(Color value) const { state_->set_color(value); }
void HoverLight::constantAttenuation(double value) const { state_->set_constant_attenuation(value); }
void HoverLight::linearAttenuation(double value) const { state_->set_linear_attenuation(value); }

struct BorderLight::State final : EdgeLight {};

impl::Beam& BorderLight::beam() const { return *state_; }

BorderLight::BorderLight() : state_{new State, /*add_ref=*/false} {}
BorderLight::BorderLight(BorderLight const& other) noexcept = default;
BorderLight& BorderLight::operator=(BorderLight const& other) noexcept = default;
BorderLight::~BorderLight() = default;

void BorderLight::size(double value) const { state_->set_size(value); }
void BorderLight::fullSize(double value) const { state_->set_full(value); }
void BorderLight::height(double value) const { state_->set_height(value); }
void BorderLight::intensity(double value) const { state_->set_intensity(value); }
void BorderLight::diffuseAmount(double value) const { state_->set_diffuse(value); }
void BorderLight::color(Color value) const { state_->set_color(value); }
void BorderLight::constantAttenuation(double value) const { state_->set_constant_attenuation(value); }
void BorderLight::linearAttenuation(double value) const { state_->set_linear_attenuation(value); }
void BorderLight::strokeThickness(double value) const { state_->set_thickness(value); }
void BorderLight::cornerRadius(CornerRadius value) const { state_->set_corner(static_cast<float>(value.topLeft)); }

struct RevealEffect::State : core::sta_refcounted {
    HoverLight hover;
    BorderLight border;
    int32_t z = 1;
};

RevealEffect::RevealEffect() : state_{new State, /*add_ref=*/false} {}
RevealEffect::RevealEffect(RevealEffect const& other) noexcept = default;
RevealEffect& RevealEffect::operator=(RevealEffect const& other) noexcept = default;
RevealEffect::~RevealEffect() = default;

void RevealEffect::setPositional(HoverLight const& value) const { state_->hover = value; }
void RevealEffect::setPositional(BorderLight const& value) const { state_->border = value; }
void RevealEffect::zIndex(int32_t value) const { state_->z = value; }

void RevealEffect::attach(FrameworkElement const& wrapper) const {
    auto const element = Object::Impl::as<xaml::FrameworkElement>(wrapper);
    auto const patch = std::make_shared<Patch>();
    patch->hover = state_->hover.state_;
    patch->border = state_->border.state_;
    patch->z = state_->z;

    element.Loaded([patch](auto const& sender, auto&&) {
        patch->place(sender.template as<xaml::FrameworkElement>());
    });
    element.Unloaded([patch](auto&&, auto&&) { patch->remove(); });
    // A button marks its own press handled; the flash takes it anyway.
    element.AddHandler(xaml::UIElement::PointerPressedEvent(),
                       winrt::box_value(xaml::Input::PointerEventHandler{[patch](auto&&, auto&&) { patch->press(); }}),
                       true);
    if (element.IsLoaded()) patch->place(element);
}

}  // namespace wxl
