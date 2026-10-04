// Опыт «Свет композитора: тень от источника».
//
// CompositionProjectedShadow: источник света, визуалы, которые тень бросают
// (клавиши, поднятые над панелью по оси Z), и визуал, который её принимает
// (панель). Тот же источник светит на сцену, так что тень и свет -- от одной
// лампы. Всё считает композитор XAML этого потока.

#include "Lab.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char16_t const* lightKinds[] = {u"PointLight", u"SpotLight", u"DistantLight"};

struct Shadows {
    // Тень принимается только от далёкого источника: на PointLight и SpotLight
    // система отвечает «Unsupported light type».
    core::observable<int> lightKind {2};
    core::observable<bool> follow {true};
    core::observable<double> height {220.0};
    core::observable<double> azimuth {120.0};
    core::observable<double> elevation {50.0};
    core::observable<Color> lightColor {rgb(255, 246, 232)};
    core::observable<bool> lit {true};
    core::observable<bool> ambientOn {true};

    core::observable<double> lift {24.0};
    core::observable<double> blurMultiplier {1.0};
    core::observable<double> minBlur {0.0};
    core::observable<double> maxBlur {40.0};
    core::observable<double> corner {22.0};
    core::observable<Color> keyColor {rgb(68, 72, 79)};
    core::observable<Color> panelColor {rgb(42, 44, 49)};

    core::observable<hstring> shadowStatus;
    core::observable<hstring> lightStatus;

    Compositor compositor;
    ContainerVisual root;
    SpriteVisual panel;
    CompositionColorBrush keyBrush;
    CompositionColorBrush panelBrush;
    AmbientLight ambient;
    PointLight point;
    SpotLight spot;
    DistantLight distant;
    std::vector<SpriteVisual> keys;
    std::vector<CompositionRoundedRectangleGeometry> outlines;
    core::nullable<CompositionProjectedShadow> shadow;
    core::nullable<CompositionPropertySet> pointer;

    Shadows()
        : compositor(CompositionTarget::getCompositorForCurrentThread()),
          root(compositor.createContainerVisual()),
          panel(compositor.createSpriteVisual()),
          keyBrush(compositor.createColorBrush(keyColor.get())),
          panelBrush(compositor.createColorBrush(panelColor.get())),
          ambient(compositor.createAmbientLight()),
          point(compositor.createPointLight()),
          spot(compositor.createSpotLight()),
          distant(compositor.createDistantLight()) {
        root.size({lab::stageWidth, lab::stageHeight});
        panel.relativeSizeAdjustment({1.0f, 1.0f});
        panel.brush(panelBrush);
        root.children().insertAtBottom(panel);

        for (int index = 0; index < lab::keyColumns * lab::keyRows; ++index) {
            auto const key = compositor.createSpriteVisual();
            key.size({lab::keySide, lab::keySide});
            key.brush(keyBrush);
            auto const outline = compositor.createRoundedRectangleGeometry();
            outline.size({lab::keySide, lab::keySide});
            key.clip(compositor.createGeometricClip(outline));
            root.children().insertAtTop(key);
            keys.push_back(key);
            outlines.push_back(outline);
        }

        point.coordinateSpace(root);
        spot.coordinateSpace(root);
        distant.coordinateSpace(root);
        spot.direction({0.0f, 0.0f, -1.0f});
        spot.innerConeAngleInDegrees(30.0f);
        spot.outerConeAngleInDegrees(60.0f);
        ambient.color(rgb(140, 146, 168));

        keyColor.on_change([this](Color const& value) noexcept { keyBrush.color(value); });
        panelColor.on_change([this](Color const& value) noexcept { panelBrush.color(value); });
        lab::onAny([this] { place(); }, lift, corner);
        lab::onAny([this] { tune(); }, blurMultiplier, minBlur, maxBlur);
        lab::onAny([this] { relight(); }, lightKind, follow, height, azimuth, elevation, lightColor, lit, ambientOn);

        place();
        cast();
        relight();
    }

    void attach(Border const& host) {
        ElementCompositionPreview::setElementChildVisual(host, root);
        pointer = ElementCompositionPreview::getPointerPositionPropertySet(host);
        relight();
    }

    // Клавиша над панелью: её высота -- третья координата смещения.
    void place() noexcept {
        float const radius = static_cast<float>(corner.get());
        for (int index = 0; index < static_cast<int>(keys.size()); ++index) {
            keys[index].offset({lab::keyLeft(index % lab::keyColumns), lab::keyTop(index / lab::keyColumns),
                                static_cast<float>(lift.get())});
            outlines[index].cornerRadius({radius, radius});
        }
    }

    void cast() noexcept {
        lab::attempt(shadowStatus, u"CreateProjectedShadow", [this] {
            auto const made = compositor.createProjectedShadow();

            auto const receiver = compositor.createProjectedShadowReceiver();
            receiver.receivingVisual(panel);
            made.receivers().add(receiver);

            for (auto const& key : keys) {
                auto const caster = compositor.createProjectedShadowCaster();
                caster.castingVisual(key);
                made.casters().insertAtTop(caster);
            }
            shadow = made;
        });
        tune();
    }

    void tune() noexcept {
        if (!shadow) {
            return;
        }
        lab::attempt(shadowStatus, u"CompositionProjectedShadow", [this] {
            shadow->blurRadiusMultiplier(static_cast<float>(blurMultiplier.get()));
            shadow->minBlurRadius(static_cast<float>(minBlur.get()));
            shadow->maxBlurRadius(static_cast<float>(std::max(maxBlur.get(), minBlur.get())));
        });
    }

    void relight() noexcept {
        lab::attempt(lightStatus, u"Источник света", [this] {
            ambient.targets().removeAll();
            point.targets().removeAll();
            spot.targets().removeAll();
            distant.targets().removeAll();
            if (lit.get() && ambientOn.get()) {
                ambient.targets().add(root);
            }

            Color const color = lightColor.get();
            switch (lightKind.get()) {
            case 0:
                point.color(color);
                move(point);
                source(point);
                break;
            case 1:
                spot.innerConeColor(color);
                spot.outerConeColor(color);
                move(spot);
                source(spot);
                break;
            default: {
                double const a = azimuth.get() * 3.14159265358979 / 180.0;
                double const e = elevation.get() * 3.14159265358979 / 180.0;
                distant.color(color);
                distant.direction({static_cast<float>(-std::cos(a) * std::cos(e)),
                                   static_cast<float>(std::sin(a) * std::cos(e)), static_cast<float>(-std::sin(e))});
                source(distant);
                break;
            }
            }
        });
    }

    // Лампа -- и источник тени, и, если включено, свет на сцене.
    void source(CompositionLight const& lamp) {
        if (lit.get()) {
            lamp.targets().add(root);
        }
        if (shadow) {
            shadow->lightSource(lamp);
        }
    }

    template <class Light>
    void move(Light const& lamp) {
        lamp.stopAnimation(u"Offset");
        float const z = static_cast<float>(height.get());
        if (follow.get() && pointer) {
            auto const track = compositor.createExpressionAnimation(u"pointer.Position + Vector3(0, 0, z)");
            track.setReferenceParameter(u"pointer", *pointer);
            track.setScalarParameter(u"z", z);
            lamp.startAnimation(u"Offset", track);
            return;
        }
        lamp.offset({lab::stageWidth / 2.0f, lab::stageHeight / 2.0f, z});
    }
};

}  // namespace

lab::Experiment lab::shadowPage() {
    auto const model = std::make_shared<Shadows>();

    auto stage = Border {
        width = double {lab::stageWidth},
        height = double {lab::stageHeight},
        hAlign.center,
        vAlign.center,
        background = SolidColorBrush {color = rgb(24, 25, 28)},
        [model](Border const& host) { model->attach(host); },
    };

    auto settings = lab::settingsPanel({
        lab::noteRow(u"Клавиши подняты над панелью; тень от них на панель бросает выбранный источник. Точечный источник и "
                     u"прожектор система источником тени не принимает."),
        lab::statusRow(model->shadowStatus),
        lab::statusRow(model->lightStatus),
        lab::group(u"Источник света", true,
                   {
                       lab::choiceRow(u"Вид", model->lightKind, lightKinds),
                       lab::toggleRow(u"Лампа за указателем (выражение)", model->follow),
                       lab::sliderRow(u"Высота над сценой, Offset.Z", model->height, 5.0, 800.0, 1.0),
                       lab::sliderRow(u"Азимут, градусы (DistantLight)", model->azimuth, 0.0, 360.0, 1.0),
                       lab::sliderRow(u"Высота, градусы (DistantLight)", model->elevation, 1.0, 90.0, 1.0),
                       lab::colorRow(u"Цвет", model->lightColor),
                       lab::toggleRow(u"Источник светит на сцену", model->lit),
                       lab::toggleRow(u"Общий свет", model->ambientOn),
                   }),
        lab::group(u"Тень", true,
                   {
                       lab::sliderRow(u"Подъём клавиш, Offset.Z", model->lift, 0.0, 120.0, 1.0),
                       lab::sliderRow(u"BlurRadiusMultiplier", model->blurMultiplier, 0.0, 8.0, 0.05),
                       lab::sliderRow(u"MinBlurRadius", model->minBlur, 0.0, 60.0, 0.5),
                       lab::sliderRow(u"MaxBlurRadius", model->maxBlur, 0.0, 120.0, 0.5),
                   }),
        lab::group(u"Клавиши", false,
                   {
                       lab::sliderRow(u"Скругление", model->corner, 0.0, 100.0, 1.0),
                       lab::colorRow(u"Цвет клавиш", model->keyColor),
                       lab::colorRow(u"Цвет панели", model->panelColor),
                   }),
    });

    return {stage, settings, model};
}
