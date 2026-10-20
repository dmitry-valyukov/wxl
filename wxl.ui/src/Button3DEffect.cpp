// wxl::Button3DEffect -- the key, its lights and its shadow; see Button3DEffect.h.
//
// The projection and the Windows headers come first, and with them every
// standard header they need: the wxl headers below carry the wxl.core import.
#include "platform.h"

#include <winrt/Microsoft.Graphics.Canvas.Effects.h>
#include <winrt/Microsoft.UI.Composition.Effects.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Graphics.Effects.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <memory>
#include <vector>

#include "Button3DEffect.h"
#include "DrawingSurface.h"
#include "Object.impl.h"
#include "ThemeBrush.h"
#include "Thickness.h"
#include <wxl/Members.h>
#include <wxl/Microsoft.UI.Composition.impl.h>
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>
#include <wxl/Microsoft.UI.Xaml.Media.h>
#include <wxl/Microsoft.UI.Xaml.impl.h>
#include "impl/effect_layer.h"
#include "impl/key_relief.h"
#include "impl/pointer_lamp.h"

namespace wxl {

namespace composition = winrt::Microsoft::UI::Composition;
namespace canvas = winrt::Microsoft::Graphics::Canvas;
namespace effects = winrt::Microsoft::Graphics::Canvas::Effects;
namespace xaml = winrt::Microsoft::UI::Xaml;

namespace {

constexpr double pi = 3.14159265358979;

// The maps are drawn twice as dense as layout units, so they stay sharp up
// to a scale of 200 %.
constexpr int density = 2;

// The outline is the button's border, one unit all round; the body lies
// inside it.
constexpr float outlineWidth = 1.0f;

// How far a press moves the inner part of the key down and right.
constexpr float pressShift = 1.0f;

// A press acts quickly, a release more slowly.
constexpr std::chrono::milliseconds pressTime{120};
constexpr std::chrono::milliseconds releaseTime{250};

// The body lies under the button's own drawing, which is zero: the button
// keeps its glyphs over the key and draws nothing else. The panel that
// receives the key's shadow lies under the body, and reaches this far past
// the key: as far as the shadow of a key raised 8 and blurred can fall.
constexpr int bodyZ = -1;
constexpr int panelZ = -2;
constexpr float panelReach = 24.0f;

// The pointer's light as a key takes it when none is written: the spot on
// the faces and the wide lamp on the rims. One pair for every Button3DEffect,
// so that the kinds of key on a keypad -- each an effect of its own -- are
// lit by the same two lamps.
HoverLight const& keyHover() {
    static HoverLight const light{dsl::size = 260.0, dsl::fullSize = 120.0, dsl::height = 30.0,
                                  dsl::intensity = 0.4};
    return light;
}

BorderLight const& keyBorder() {
    static BorderLight const light{dsl::size = 340.0, dsl::fullSize = 200.0, dsl::height = 60.0,
                                   dsl::intensity = 1.5};
    return light;
}

// The intensity goes to where it belongs from wherever it is: one key
// frame, at the end.
composition::ScalarKeyFrameAnimation toward(composition::Compositor const& compositor, float value,
                                            std::chrono::milliseconds time) {
    auto const to = compositor.CreateScalarKeyFrameAnimation();
    to.InsertKeyFrame(1.0f, value);
    to.Duration(time);
    return to;
}

composition::CompositionEffectSourceParameter source(wchar_t const* name) {
    return composition::CompositionEffectSourceParameter{name};
}

// The fixed light of one island: the light from all around and the distant
// lamp, aimed at every key of the effect on that island.
struct Scene {
    winrt::weak_ref<xaml::UIElement> root;
    composition::AmbientLight ambient{nullptr};
    composition::DistantLight distant{nullptr};
};

// The pictures of one size of key, shared by every key of that size: the
// maps, the masks and the brushes made of them.
struct Maps {
    int width, height;  // pixels
    DrawingSurface normals, pressedNormals, shape, faceShape, holeShadow;
    composition::CompositionEffectBrush rim{nullptr}, pressedRim{nullptr}, face{nullptr};
    int users = 0;
};

}  // namespace

struct Button3DEffect::State : core::sta_refcounted {
    struct Patch;

    Color ink = rgb(237, 239, 242);
    Color albedo = rgb(68, 72, 79);
    double specularAmount = 0.15;
    double specularShine = 30.0;

    double emboss = -1.0;
    double rim = 3.0;
    double slope = 1.0;
    bool rounded = false;
    double smooth = 2.0;
    double rise = 3.0;
    double travel = 5.0;

    double shadow = 0.7;
    double azimuth = 120.0;
    double elevation = 50.0;
    Color lampColor = rgb(255, 255, 255);
    Color ambientColor = rgb(255, 255, 255);
    double castBlur = 1.0;
    double holeShadow = 1.0;
    double holeSoftness = 1.0;

    bool pressDivesLamp = true;
    bool pressFlashes = false;
    bool pressMovesLabel = true;
    bool pressShades = false;
    double diveDepth = 0.33;
    double diveGain = 2.0;
    double shadeAmount = 0.2;
    double labelShade = 0.15;

    HoverLight hover = keyHover();
    BorderLight border = keyBorder();

    std::vector<Scene> scenes;
    std::vector<Maps> maps;
    // The keys attached and on screen, by bare pointer: a patch leaves the
    // list before it goes.
    std::vector<Patch*> patches;
    composition::CompositionColorBrush base{nullptr};
    composition::CompositionColorBrush shadowBrush{nullptr};
    // The glyphs of a pressed key: one brush for every key of the effect,
    // made with the first key -- a XAML brush cannot be made before the
    // framework is up, and an effect may be written at namespace scope.
    core::nullable<SolidColorBrush> pressedInk;
    composition::CompositionEffectFactory plain{nullptr};
    composition::CompositionEffectFactory pressed{nullptr};

    // --- the lights

    Scene& scene_of(xaml::UIElement const& root) {
        std::erase_if(scenes, [](Scene const& scene) { return !scene.root.get(); });
        for (Scene& scene : scenes) {
            if (scene.root.get() == root) return scene;
        }
        auto const space = xaml::Hosting::ElementCompositionPreview::GetElementVisual(root);
        Scene scene;
        scene.root = root;
        scene.ambient = space.Compositor().CreateAmbientLight();
        scene.distant = space.Compositor().CreateDistantLight();
        scene.distant.CoordinateSpace(space);
        scenes.push_back(std::move(scene));
        relight(scenes.back());
        return scenes.back();
    }

    // A flat spot keeps the material's colour under any share: ambient gives
    // 1 - shadow, the lamp on a flat spot shadow. The lamp stands at the
    // azimuth and elevation; the light comes from it.
    void relight(Scene const& scene) const {
        double const a = azimuth * pi / 180.0;
        double const e = elevation * pi / 180.0;
        scene.ambient.Color(std::bit_cast<winrt::Windows::UI::Color>(ambientColor));
        scene.ambient.Intensity(static_cast<float>(1.0 - shadow));
        scene.distant.Color(std::bit_cast<winrt::Windows::UI::Color>(lampColor));
        scene.distant.Intensity(static_cast<float>(shadow / std::sin(e)));
        scene.distant.Direction({static_cast<float>(-std::cos(a) * std::cos(e)),
                                 static_cast<float>(std::sin(a) * std::cos(e)), static_cast<float>(-std::sin(e))});
    }

    void relight() {
        for (Scene const& scene : scenes) relight(scene);
        // The cast shadow takes the lamp's share off the panel: in it, ambient
        // light alone is left.
        if (shadowBrush) {
            shadowBrush.Color({static_cast<uint8_t>(std::lround(shadow * 255.0)), 0, 0, 0});
        }
        reshade();
    }

    // --- the pictures

    impl::KeyRelief relief_of(float width, float height, float corner, bool pressedKey) const {
        impl::KeyRelief key;
        key.width = width;
        key.height = height;
        key.corner = corner;
        key.rim = rim;
        key.slope = slope;
        key.rounded = rounded;
        key.emboss = emboss;
        key.smooth = smooth;
        if (pressedKey && pressMovesLabel) {
            key.shiftX = pressShift;
            key.shiftY = pressShift;
        }
        return key;
    }

    impl::HoleShadow hole() const {
        impl::HoleShadow shadowOfEdge;
        shadowOfEdge.depth = travel - rise;
        shadowOfEdge.azimuth = azimuth;
        shadowOfEdge.elevation = elevation;
        shadowOfEdge.share = shadow;
        shadowOfEdge.amount = holeShadow;
        shadowOfEdge.soft = holeSoftness;
        return shadowOfEdge;
    }

    // The maps live in pixels; the shape they picture is the key in layout
    // units, and its rounding is the body's.
    void draw(Maps const& made) const {
        float const width = static_cast<float>(made.width) / density, height = static_cast<float>(made.height) / density;
        float const corner = corner_of(made);
        impl::draw_key_map(made.normals, relief_of(width, height, corner, false));
        impl::draw_face_mask(made.faceShape, relief_of(width, height, corner, false));
        impl::draw_shape(made.shape, corner * density);
        impl::draw_key_map(made.pressedNormals, relief_of(width, height, corner, true));
        shade(made);
    }

    void shade(Maps const& made) const {
        float const width = static_cast<float>(made.width) / density, height = static_cast<float>(made.height) / density;
        impl::draw_hole_shadow(made.holeShadow, relief_of(width, height, corner_of(made), true), hole());
    }

    // Keys of one size share one rounding too: the maps are looked up by
    // size and the rounding is read off the first key of that size.
    float corner_of(Maps const& made) const;

    // The graph of a key: the colour, multiplied by ambient and diffuse
    // light, plus the gleam, not coloured; on top the cut by the alpha of
    // the shape. The sum adds the alphas too -- one and one make two -- and
    // without the clamp the half-transparent edge of the face mask would
    // take away the rim under it. The pressed key lays the shadow of the
    // opening's edge over the light before the cut.
    void make_factories(composition::Compositor const& compositor) {
        auto const lit = [&] {
            composition::Effects::SceneLightingEffect matte;
            matte.Name(L"Matte");
            matte.NormalMapSource(source(L"NormalMap"));
            matte.AmbientAmount(1.0f);
            matte.DiffuseAmount(1.0f);
            matte.SpecularAmount(0.0f);

            composition::Effects::SceneLightingEffect gloss;
            gloss.Name(L"Gloss");
            gloss.NormalMapSource(source(L"NormalMap"));
            gloss.AmbientAmount(0.0f);
            gloss.DiffuseAmount(0.0f);
            gloss.SpecularAmount(static_cast<float>(specularAmount));
            gloss.SpecularShine(static_cast<float>(specularShine));

            effects::ArithmeticCompositeEffect tinted;
            tinted.Source1(source(L"Base"));
            tinted.Source2(matte);
            tinted.MultiplyAmount(1.0f);
            tinted.Source1Amount(0.0f);
            tinted.Source2Amount(0.0f);

            effects::ArithmeticCompositeEffect whole;
            whole.Source1(tinted);
            whole.Source2(gloss);
            whole.MultiplyAmount(0.0f);
            whole.Source1Amount(1.0f);
            whole.Source2Amount(1.0f);
            whole.ClampOutput(true);
            return whole;
        };

        auto const cut = [&](winrt::Windows::Graphics::Effects::IGraphicsEffectSource const& body) {
            effects::CompositeEffect shaped;
            shaped.Mode(canvas::CanvasComposite::DestinationIn);
            shaped.Sources().Append(body);
            shaped.Sources().Append(source(L"Shape"));
            return compositor.CreateEffectFactory(shaped);
        };

        effects::CompositeEffect shaded;
        shaded.Mode(canvas::CanvasComposite::SourceOver);
        shaded.Sources().Append(lit());
        shaded.Sources().Append(source(L"HoleShadow"));

        plain = cut(lit());
        pressed = cut(shaded);
    }

    composition::CompositionSurfaceBrush picture(DrawingSurface const& surface) const {
        auto const brush = Object::Impl::as<composition::CompositionSurfaceBrush>(surface.brush());
        brush.Stretch(composition::CompositionStretch::Fill);
        return brush;
    }

    void dress(Maps& made) const {
        auto const brushOf = [&](composition::CompositionEffectFactory const& factory, DrawingSurface const& normals,
                                 DrawingSurface const& shape) {
            auto const brush = factory.CreateBrush();
            brush.SetSourceParameter(L"NormalMap", picture(normals));
            brush.SetSourceParameter(L"Base", base);
            brush.SetSourceParameter(L"Shape", picture(shape));
            return brush;
        };
        made.rim = brushOf(plain, made.normals, made.shape);
        made.pressedRim = brushOf(pressed, made.pressedNormals, made.shape);
        made.pressedRim.SetSourceParameter(L"HoleShadow", picture(made.holeShadow));
        made.face = brushOf(plain, made.normals, made.faceShape);
    }

    int maps_for(composition::Compositor const& compositor, int width, int height) {
        for (std::size_t index = 0; index < maps.size(); ++index) {
            if (maps[index].width == width && maps[index].height == height) {
                ++maps[index].users;
                return static_cast<int>(index);
            }
        }
        if (!base) {
            base = compositor.CreateColorBrush(std::bit_cast<winrt::Windows::UI::Color>(albedo));
            shadowBrush = compositor.CreateColorBrush({static_cast<uint8_t>(std::lround(shadow * 255.0)), 0, 0, 0});
            make_factories(compositor);
        }
        Compositor const wrapped = Object::Impl::wrap<Compositor>(composition::Compositor{compositor});
        SizeInt32 const size{width, height};
        maps.push_back(Maps{width, height, DrawingSurface{wrapped, size}, DrawingSurface{wrapped, size},
                            DrawingSurface{wrapped, size}, DrawingSurface{wrapped, size}, DrawingSurface{wrapped, size}});
        maps.back().users = 1;
        dress(maps.back());
        return static_cast<int>(maps.size() - 1);
    }

    // The pictures of a size nobody wears any more go; the patches that
    // wear later ones move down with them.
    void release_maps(int index);

    // The glyphs of a pressed key keep 1 - labelShade of their colour.
    Color pressedInkColor() const { return color_helper::shade(ink, 1.0 - labelShade); }

    void redye() const {
        if (pressedInk) pressedInk->color(pressedInkColor());
    }

    void reshape();
    void reshade() {
        for (Maps const& made : maps) shade(made);
    }
    void repaint();
    void reblur();
    void refit();
};

// One attached key: its body, made on Loaded, taken off on Unloaded. The
// element's own events hold the handlers that hold this.
struct Button3DEffect::State::Patch : std::enable_shared_from_this<Patch> {
    core::intrusive_ptr<State> state;
    winrt::weak_ref<xaml::FrameworkElement> element;
    winrt::weak_ref<xaml::UIElement> root;
    // Bottom up: the panel, a clear sprite wider than the key that takes
    // its shadow; the outline, outside the light; the rim; the face over its
    // middle, so that the join of face and rim shows no gap to the outline;
    // the shade of a pressed key, clear until then.
    composition::SpriteVisual panel{nullptr};
    composition::SpriteVisual outline{nullptr};
    composition::SpriteVisual rim{nullptr};
    composition::SpriteVisual face{nullptr};
    composition::SpriteVisual shade{nullptr};
    composition::CompositionRoundedRectangleGeometry outlineShape{nullptr};
    composition::CompositionRoundedRectangleGeometry shadeShape{nullptr};
    composition::CompositionProjectedShadow cast{nullptr};
    composition::CompositionProjectedShadowCaster caster{nullptr};
    int mapsIndex = -1;
    float bodyWidth = 0.0f, bodyHeight = 0.0f, bodyCorner = 0.0f;
    bool pressed = false;
    xaml::FrameworkElement::SizeChanged_revoker sized;
    int64_t pressedToken = 0;

    composition::Compositor compositor() const { return outline.Compositor(); }

    void place(xaml::FrameworkElement const& fe) {
        if (outline) return;
        auto const xamlRoot = fe.XamlRoot();
        if (!xamlRoot) return;
        auto const island = xamlRoot.Content();
        if (!island) return;
        root = island;
        element = fe;

        auto const host = xaml::Hosting::ElementCompositionPreview::GetElementVisual(fe);
        auto const compositor = host.Compositor();

        // The shadow the compositor projects lands on a receiver whatever the
        // receiver draws, and it is drawn over the receiver's whole tree: the
        // panel is the key's own, under its body, so the body covers its own
        // shadow, and the keys after it in their parent cover what falls on
        // them. A shadow thrown back over an earlier key is drawn over it.
        panel = compositor.CreateSpriteVisual();
        panel.Brush(compositor.CreateColorBrush({0, 0, 0, 0}));

        outlineShape = compositor.CreateRoundedRectangleGeometry();
        outline = compositor.CreateSpriteVisual();
        outline.Brush(compositor.CreateColorBrush({255, 0, 0, 0}));
        outline.Clip(compositor.CreateGeometricClip(outlineShape));

        // Hard borders: with soft ones the normals' answer to the light is
        // turned. The smooth edge of the shape comes from the mask in the
        // graph.
        rim = compositor.CreateSpriteVisual();
        rim.BorderMode(composition::CompositionBorderMode::Hard);
        face = compositor.CreateSpriteVisual();
        face.BorderMode(composition::CompositionBorderMode::Hard);

        shadeShape = compositor.CreateRoundedRectangleGeometry();
        shade = compositor.CreateSpriteVisual();
        shade.Brush(compositor.CreateColorBrush({255, 0, 0, 0}));
        shade.Opacity(0.0f);
        shade.Clip(compositor.CreateGeometricClip(shadeShape));

        bodyCorner = std::max(impl::corner_of(fe) - outlineWidth, 0.0f);
        state->patches.push_back(this);
        fit(static_cast<float>(fe.ActualWidth()), static_cast<float>(fe.ActualHeight()));

        // Aimed at before they are shown: see impl::Lamp::shining.
        Scene const& scene = state->scene_of(island);
        for (auto const& sprite : {rim, face}) {
            scene.ambient.Targets().Add(sprite);
            scene.distant.Targets().Add(sprite);
        }
        state->hover.beam().light_up(island, face);
        state->border.beam().light_up(island, rim);

        // The shadow on the panel: from the distant lamp, cast by the rim --
        // it is the whole key. One shadow per key: of one shadow's casters the
        // system respects only so many.
        cast = compositor.CreateProjectedShadow();
        cast.LightSource(scene.distant);
        auto const receiver = compositor.CreateProjectedShadowReceiver();
        receiver.ReceivingVisual(panel);
        cast.Receivers().Add(receiver);
        caster = compositor.CreateProjectedShadowCaster();
        caster.CastingVisual(rim);
        caster.Brush(state->shadowBrush);
        cast.Casters().InsertAtTop(caster);
        blur();

        impl::when_drawn(host.as<composition::ContainerVisual>(),
                         [weak = weak_from_this(), layer = outline](auto const& children) {
                             auto const self = weak.lock();
                             // Unloaded before the frame came: nothing to put there.
                             if (!self || self->outline != layer) return;
                             impl::insert_layer(children, self->panel, panelZ);
                             for (auto const& sprite : {self->outline, self->rim, self->face, self->shade}) {
                                 impl::insert_layer(children, sprite, bodyZ);
                             }
                         });

        sized = fe.SizeChanged(winrt::auto_revoke, [weak = weak_from_this()](auto&&, auto const& args) {
            if (auto const self = weak.lock()) {
                auto const size = args.NewSize();
                self->fit(static_cast<float>(size.Width), static_cast<float>(size.Height));
            }
        });
        pressedToken = fe.RegisterPropertyChangedCallback(
            xaml::Controls::Primitives::ButtonBase::IsPressedProperty(),
            [weak = weak_from_this()](xaml::DependencyObject const& sender, auto&&) {
                if (auto const self = weak.lock()) {
                    self->set_pressed(sender.as<xaml::Controls::Primitives::ButtonBase>().IsPressed());
                }
            });
    }

    // The body fills the button inside its outline; the pictures are those
    // of its size.
    void fit(float width, float height) {
        bodyWidth = std::max(width - 2.0f * outlineWidth, 0.0f);
        bodyHeight = std::max(height - 2.0f * outlineWidth, 0.0f);
        panel.Size({width + 2.0f * panelReach, height + 2.0f * panelReach});
        panel.Offset({-panelReach, -panelReach, 0.0f});
        outline.Size({width, height});
        outlineShape.Size({width, height});
        outlineShape.CornerRadius({bodyCorner + outlineWidth, bodyCorner + outlineWidth});
        float const inner = outlineWidth + (pressed && state->pressMovesLabel ? pressShift : 0.0f);
        for (auto const& sprite : {rim, shade}) {
            sprite.Size({bodyWidth, bodyHeight});
            sprite.Offset({outlineWidth, outlineWidth, sprite == rim ? standing() : 0.0f});
        }
        face.Size({bodyWidth, bodyHeight});
        face.Offset({inner, inner, 0.0f});
        shadeShape.Size({bodyWidth, bodyHeight});
        shadeShape.CornerRadius({bodyCorner, bodyCorner});

        int const pixelsWide = static_cast<int>(std::lround(bodyWidth * density));
        int const pixelsHigh = static_cast<int>(std::lround(bodyHeight * density));
        if (mapsIndex >= 0 && state->maps[mapsIndex].width == pixelsWide &&
            state->maps[mapsIndex].height == pixelsHigh) {
            return;
        }
        int const was = mapsIndex;
        mapsIndex = -1;
        state->release_maps(was);
        bool const fresh = std::ranges::none_of(
            state->maps, [&](Maps const& made) { return made.width == pixelsWide && made.height == pixelsHigh; });
        mapsIndex = state->maps_for(compositor(), pixelsWide, pixelsHigh);
        // Drawn after the patch wears it: the drawing reads the rounding off
        // the patches of that size.
        if (fresh) state->draw(state->maps[mapsIndex]);
        wear();
    }

    void wear() const {
        if (mapsIndex < 0) return;
        Maps const& made = state->maps[mapsIndex];
        rim.Brush(pressed ? made.pressedRim : made.rim);
        face.Brush(made.face);
    }

    void blur() const {
        if (!cast) return;
        cast.BlurRadiusMultiplier(static_cast<float>(state->castBlur));
        cast.MinBlurRadius(0.0f);
        cast.MaxBlurRadius(8.0f);
    }

    // How high the key stands over the panel: `rise` at rest; pressed, down
    // by its travel but not below the panel -- what is below casts no shadow
    // onto it.
    float standing() const {
        return static_cast<float>(pressed ? std::max(state->rise - state->travel, 0.0) : state->rise);
    }

    void set_pressed(bool down) {
        if (pressed == down || !outline) return;
        pressed = down;
        auto const time = down ? pressTime : releaseTime;
        auto const compositor = this->compositor();
        State const& s = *state;

        wear();
        rim.StartAnimation(L"Offset.Z", toward(compositor, standing(), time));

        // The release puts back everything a press could have touched,
        // whatever the flags say now.
        if (!down || s.pressShades) {
            shade.StartAnimation(L"Opacity", toward(compositor, down ? static_cast<float>(s.shadeAmount) : 0.0f, time));
        }
        if (!down || s.pressMovesLabel) {
            float const by = down ? pressShift : 0.0f;
            face.Offset({outlineWidth + by, outlineWidth + by, 0.0f});
            if (auto const fe = element.get()) {
                if (xaml::Media::VisualTreeHelper::GetChildrenCount(fe) > 0) {
                    if (auto const content = xaml::Media::VisualTreeHelper::GetChild(fe, 0).try_as<xaml::UIElement>()) {
                        content.Translation({by, by, 0.0f});
                    }
                }
            }
        }

        // The spot's lamp: lowered and brightened by the dive, flashed by the
        // flash; the pointer's own fading is the beam's business.
        auto const island = root.get();
        impl::Beam& beam = s.hover.beam();
        impl::Lamp const* const lamp = island ? beam.lamp_of(island) : nullptr;
        if (!lamp) return;
        bool const dives = down && s.pressDivesLamp;
        float const height = beam.height * (dives ? static_cast<float>(s.diveDepth) : 1.0f);
        if (!down || s.pressDivesLamp) {
            lamp->light.Properties().StartAnimation(impl::lampHeightProperty, toward(compositor, height, time));
        }
        if (!lamp->shining) return;
        float const intensity = beam.intensity * (dives ? static_cast<float>(s.diveGain) : 1.0f);
        if (down && s.pressFlashes) {
            auto const pulse = compositor.CreateScalarKeyFrameAnimation();
            pulse.InsertKeyFrame(0.0f, intensity * impl::lampFlashGain);
            pulse.InsertKeyFrame(1.0f, intensity);
            pulse.Duration(impl::lampFlashTime);
            lamp->light.StartAnimation(L"InnerConeIntensity", pulse);
            lamp->light.StartAnimation(L"OuterConeIntensity", pulse);
        } else {
            auto const to = toward(compositor, intensity, time);
            lamp->light.StartAnimation(L"InnerConeIntensity", to);
            lamp->light.StartAnimation(L"OuterConeIntensity", to);
        }
    }

    void remove() {
        if (!outline) return;
        sized.revoke();
        if (auto const fe = element.get()) {
            fe.UnregisterPropertyChangedCallback(xaml::Controls::Primitives::ButtonBase::IsPressedProperty(),
                                                 pressedToken);
        }
        for (auto const& sprite : {panel, outline, rim, face, shade}) {
            if (auto const parent = sprite.Parent()) parent.Children().Remove(sprite);
        }
        if (auto const island = root.get()) {
            state->hover.beam().leave(island, face);
            state->border.beam().leave(island, rim);
            for (Scene& scene : state->scenes) {
                if (scene.root.get() != island) continue;
                for (auto const& sprite : {rim, face}) {
                    scene.ambient.Targets().Remove(sprite);
                    scene.distant.Targets().Remove(sprite);
                }
            }
            std::erase_if(state->scenes, [](Scene const& scene) { return scene.ambient.Targets().Count() == 0; });
        }
        std::erase(state->patches, this);
        int const was = mapsIndex;
        mapsIndex = -1;
        state->release_maps(was);
        cast = nullptr;
        caster = nullptr;
        panel = nullptr;
        outline = nullptr;
        rim = nullptr;
        face = nullptr;
        shade = nullptr;
        pressed = false;
    }

    ~Patch() { remove(); }
};

float Button3DEffect::State::corner_of(Maps const& made) const {
    for (Patch const* patch : patches) {
        if (patch->mapsIndex >= 0 && &maps[patch->mapsIndex] == &made) return patch->bodyCorner;
    }
    return 0.0f;
}

void Button3DEffect::State::release_maps(int index) {
    if (index < 0 || --maps[index].users > 0) return;
    maps.erase(maps.begin() + index);
    for (Patch* patch : patches) {
        if (patch->mapsIndex > index) --patch->mapsIndex;
    }
}

void Button3DEffect::State::reshape() {
    for (Maps const& made : maps) draw(made);
}

void Button3DEffect::State::repaint() {
    if (!base) return;
    base.Color(std::bit_cast<winrt::Windows::UI::Color>(albedo));
    make_factories(base.Compositor());
    for (Maps& made : maps) dress(made);
    for (Patch const* patch : patches) patch->wear();
}

void Button3DEffect::State::reblur() {
    for (Patch const* patch : patches) patch->blur();
}

void Button3DEffect::State::refit() {
    for (Patch* patch : patches) {
        if (auto const fe = patch->element.get()) {
            patch->fit(static_cast<float>(fe.ActualWidth()), static_cast<float>(fe.ActualHeight()));
        }
    }
}

Button3DEffect::Button3DEffect() : state_{new State, /*add_ref=*/false} {}
Button3DEffect::Button3DEffect(Button3DEffect const& other) noexcept = default;
Button3DEffect& Button3DEffect::operator=(Button3DEffect const& other) noexcept = default;
Button3DEffect::~Button3DEffect() = default;

// The material: the colour of the glyphs is written on the button when the
// effect is attached; the rest reaches the keys already attached.
void Button3DEffect::foreground(Color value) const {
    state_->ink = value;
    state_->redye();
}
void Button3DEffect::background(Color value) const {
    state_->albedo = value;
    state_->repaint();
}
void Button3DEffect::specularAmount(double value) const {
    state_->specularAmount = std::max(value, 0.0);
    state_->repaint();
}
void Button3DEffect::specularShine(double value) const {
    state_->specularShine = std::max(value, 1.0);
    state_->repaint();
}

// The shape: the pictures are drawn anew.
void Button3DEffect::emboss(double value) const {
    state_->emboss = std::clamp(value, -1.0, 1.0);
    state_->reshape();
}
void Button3DEffect::rim(double value) const {
    state_->rim = std::max(value, 0.0);
    state_->reshape();
}
void Button3DEffect::slope(double value) const {
    state_->slope = std::max(value, 0.0);
    state_->reshape();
}
void Button3DEffect::rounded(bool value) const {
    state_->rounded = value;
    state_->reshape();
}
void Button3DEffect::smooth(double value) const {
    state_->smooth = std::max(value, 0.0);
    state_->reshape();
}
void Button3DEffect::rise(double value) const {
    state_->rise = std::max(value, 0.0);
    state_->reshade();
    state_->refit();
}
void Button3DEffect::travel(double value) const {
    state_->travel = std::max(value, 0.0);
    state_->reshade();
    state_->refit();
}

// The light.
void Button3DEffect::shadow(double value) const {
    state_->shadow = std::clamp(value, 0.0, 1.0);
    state_->relight();
}
void Button3DEffect::azimuth(double value) const {
    state_->azimuth = value;
    state_->relight();
}
void Button3DEffect::elevation(double value) const {
    state_->elevation = std::clamp(value, 1.0, 90.0);
    state_->relight();
}
void Button3DEffect::lampColor(Color value) const {
    state_->lampColor = value;
    state_->relight();
}
void Button3DEffect::ambientColor(Color value) const {
    state_->ambientColor = value;
    state_->relight();
}
void Button3DEffect::castBlur(double value) const {
    state_->castBlur = std::max(value, 0.0);
    state_->reblur();
}
void Button3DEffect::holeShadow(double value) const {
    state_->holeShadow = std::clamp(value, 0.0, 1.0);
    state_->reshade();
}
void Button3DEffect::holeSoftness(double value) const {
    state_->holeSoftness = std::max(value, 0.0);
    state_->reshade();
}

// The press: read when a key is pressed, except the shift, which is drawn
// into the pressed key's pictures.
void Button3DEffect::pressDivesLamp(bool value) const { state_->pressDivesLamp = value; }
void Button3DEffect::pressFlashes(bool value) const { state_->pressFlashes = value; }
void Button3DEffect::pressMovesLabel(bool value) const {
    state_->pressMovesLabel = value;
    state_->reshape();
}
void Button3DEffect::pressShades(bool value) const { state_->pressShades = value; }
void Button3DEffect::diveDepth(double value) const { state_->diveDepth = std::clamp(value, 0.0, 1.0); }
void Button3DEffect::diveGain(double value) const { state_->diveGain = std::max(value, 0.0); }
void Button3DEffect::shadeAmount(double value) const { state_->shadeAmount = std::clamp(value, 0.0, 1.0); }
void Button3DEffect::labelShade(double value) const {
    state_->labelShade = std::clamp(value, 0.0, 1.0);
    state_->redye();
}

void Button3DEffect::setPositional(HoverLight const& value) const { state_->hover = value; }
void Button3DEffect::setPositional(BorderLight const& value) const { state_->border = value; }

void Button3DEffect::attach(Button const& button) const {
    State& s = *state_;
    if (!s.pressedInk) s.pressedInk = SolidColorBrush{s.pressedInkColor()};

    // The button draws its glyphs and nothing else: the body is the
    // effect's, under them, and the outline is drawn with it. A clear
    // background keeps the button under the pointer; the border keeps its
    // width for the glyphs' sake and shows nothing.
    SolidColorBrush const clear{rgba(0, 0, 0, 0.0)};
    Apply{
        button,
        dsl::foreground = SolidColorBrush{s.ink},
        dsl::background = clear,
        ThemeBrush{u"ButtonBackgroundPointerOver", clear},
        ThemeBrush{u"ButtonBackgroundPressed", clear},
        ThemeBrush{u"ButtonForegroundPointerOver", SolidColorBrush{s.ink}},
        ThemeBrush{u"ButtonForegroundPressed", *s.pressedInk},
        ThemeBrush{u"ButtonBorderBrushPointerOver", clear},
        ThemeBrush{u"ButtonBorderBrushPressed", clear},
        dsl::borderBrush = clear,
        BorderThickness{outlineWidth},
    };

    auto const element = Object::Impl::as<xaml::FrameworkElement>(button);
    auto const patch = std::make_shared<State::Patch>();
    patch->state = state_;
    element.Loaded([patch](auto const& sender, auto&&) {
        patch->place(sender.template as<xaml::FrameworkElement>());
    });
    element.Unloaded([patch](auto&&, auto&&) { patch->remove(); });
    if (element.IsLoaded()) patch->place(element);
}

}  // namespace wxl
