// Опыт «Свет композитора: прожектор за указателем».
//
// Клавиши -- кнопки XAML. У каждой визуалом-ребёнком два спрайта: лицо и
// рамка. Оба закрашены кистью эффекта с SceneLightingEffect, чей свет матрица
// цвета переводит в альфу белого: где прожектор светит, спрайт белеет, где
// нет -- прозрачен. Рамку из такого же света вырезает маска-кольцо, растянутая
// кистью девяти частей. Прожекторов два, оба стоят над указателем: узкий
// нацелен на лица, широкий -- на рамки, так что светятся и рамки соседей.
// Место прожекторов считает композитор -- выражением от набора свойств
// указателя над клавиатурой.

#include "Lab.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Маска-кольцо рисуется вдвое плотнее DIP и растягивается с масштабом 0.5.
constexpr int ringPixels = 128;
constexpr float ringScale = 2.0f;

// Яркость света в альфу, цвет как есть; gain усиливает и то и другое.
Matrix5x4 lightToAlpha(float gain) {
    Matrix5x4 matrix {};
    matrix.m11 = gain;
    matrix.m22 = gain;
    matrix.m33 = gain;
    matrix.m14 = gain * 0.2125f;
    matrix.m24 = gain * 0.7154f;
    matrix.m34 = gain * 0.0721f;
    return matrix;
}

struct Reveal {
    // Пятно на лице клавиши.
    core::observable<double> faceHeight {120.0};
    core::observable<double> faceRadius {70.0};
    core::observable<double> faceIntensity {1.0};
    core::observable<double> faceDiffuse {0.35};

    // Свет на рамках.
    core::observable<double> rimHeight {120.0};
    core::observable<double> rimRadius {170.0};
    core::observable<double> rimIntensity {1.0};
    core::observable<double> rimDiffuse {0.8};
    core::observable<double> rimThickness {1.5};
    core::observable<double> corner {4.0};

    core::observable<Color> lightColor {rgb(255, 255, 255)};
    core::observable<double> constantAttenuation {1.0};
    core::observable<double> linearAttenuation {0.0};

    core::observable<hstring> brushStatus;
    core::observable<hstring> lightStatus;

    Compositor compositor;
    SpotLight faceLight;
    SpotLight rimLight;
    DrawingSurface ring;
    CompositionNineGridBrush frame;
    std::vector<SpriteVisual> faces;
    std::vector<SpriteVisual> rims;
    std::vector<CompositionRoundedRectangleGeometry> outlines;
    core::nullable<CompositionPropertySet> pointer;
    // Указатель над клавиатурой. Вне её прожекторы не гаснут, а светят с нулевой
    // силой: спрайт, на который не нацелен ни один включённый источник,
    // SceneLightingEffect закрашивает белым целиком.
    bool shining = false;

    Reveal()
        : compositor(CompositionTarget::getCompositorForCurrentThread()),
          faceLight(compositor.createSpotLight()),
          rimLight(compositor.createSpotLight()),
          ring(compositor, SizeInt32 {ringPixels, ringPixels}),
          frame(compositor.createNineGridBrush()) {
        auto const mask = ring.brush();
        mask.stretch(CompositionStretch::Fill);
        frame.source(mask);
        frame.setInsetScales(1.0f / ringScale);

        faceLight.direction({0.0f, 0.0f, -1.0f});
        rimLight.direction({0.0f, 0.0f, -1.0f});

        lab::onAny([this] { paint(); }, faceDiffuse, rimDiffuse);
        lab::onAny([this] { outline(); }, rimThickness, corner);
        lab::onAny([this] { relight(); }, faceHeight, faceRadius, faceIntensity, rimHeight, rimRadius, rimIntensity,
                   lightColor, constantAttenuation, linearAttenuation);
    }

    // Вызывается из скобок кнопки, до того как она встала в клавиатуру.
    void attachKey(Button const& key) {
        auto const holder = compositor.createContainerVisual();
        holder.relativeSizeAdjustment({1.0f, 1.0f});

        // Скругление лица следует за размером кнопки: выражение от визуала,
        // который XAML ведёт сам.
        auto const outlineOfKey = compositor.createRoundedRectangleGeometry();
        auto const follow = compositor.createExpressionAnimation(u"key.Size");
        follow.setReferenceParameter(u"key", ElementCompositionPreview::getElementVisual(key));
        outlineOfKey.startAnimation(u"Size", follow);

        auto const face = compositor.createSpriteVisual();
        face.relativeSizeAdjustment({1.0f, 1.0f});
        face.clip(compositor.createGeometricClip(outlineOfKey));

        auto const rim = compositor.createSpriteVisual();
        rim.relativeSizeAdjustment({1.0f, 1.0f});

        holder.children().insertAtTop(face);
        holder.children().insertAtTop(rim);
        ElementCompositionPreview::setElementChildVisual(key, holder);

        faces.push_back(face);
        rims.push_back(rim);
        outlines.push_back(outlineOfKey);
    }

    // Клавиатура на экране: её визуал -- пространство координат прожекторов,
    // её набор свойств указателя -- их место.
    void connect(Grid const& pad) {
        auto const space = ElementCompositionPreview::getElementVisual(pad);
        pointer = ElementCompositionPreview::getPointerPositionPropertySet(pad);
        outline();
        paint();
        lab::attempt(lightStatus, u"Прожекторы", [&] {
            faceLight.coordinateSpace(space);
            rimLight.coordinateSpace(space);
            faceLight.targets().removeAll();
            rimLight.targets().removeAll();
            for (auto const& face : faces) {
                faceLight.targets().add(face);
            }
            for (auto const& rim : rims) {
                rimLight.targets().add(rim);
            }
        });
        relight();
    }

    void shine(bool on) noexcept {
        shining = on;
        relight();
    }

    // Нажатие: пятно вспыхивает и гаснет до прежней силы.
    void flash() noexcept {
        if (!shining) {
            return;
        }
        lab::attempt(lightStatus, u"Вспышка", [this] {
            float const rest = static_cast<float>(faceIntensity.get());
            auto const pulse = compositor.createScalarKeyFrameAnimation();
            pulse.insertKeyFrame(0.0f, rest * 3.0f);
            pulse.insertKeyFrame(1.0f, rest);
            pulse.duration(std::chrono::milliseconds {400});
            faceLight.startAnimation(u"InnerConeIntensity", pulse);
            faceLight.startAnimation(u"OuterConeIntensity", pulse);
        });
    }

    void outline() noexcept {
        lab::attempt(brushStatus, u"Маска рамки", [this] {
            float const radius = static_cast<float>(corner.get());
            float const thickness = static_cast<float>(rimThickness.get());
            lab::drawRing(ring, radius * ringScale, thickness * ringScale);
            // Углы и края маски -- по радиусу и толщине, середина тянется.
            frame.setInsets(std::min((radius + thickness) * ringScale, ringPixels / 2.0f - 1.0f));
            for (auto const& outlineOfKey : outlines) {
                outlineOfKey.cornerRadius({radius, radius});
            }
        });
    }

    void paint() noexcept {
        lab::attempt(brushStatus, u"CreateEffectFactory с SceneLightingEffect и ColorMatrixEffect", [this] {
            auto const glow = [](float diffuse, float gain) {
                SceneLightingEffect light;
                light.ambientAmount(0.0f);
                light.diffuseAmount(diffuse);
                light.specularAmount(0.0f);

                ColorMatrixEffect toAlpha;
                toAlpha.source(light);
                toAlpha.colorMatrix(lightToAlpha(gain));
                toAlpha.alphaMode(CanvasAlphaMode::Straight);
                return toAlpha;
            };

            auto const faceBrush =
                compositor.createEffectFactory(glow(static_cast<float>(faceDiffuse.get()), 1.0f)).createBrush();

            // Свет рамки, вырезанный маской: от нижнего входа остаётся то, что
            // под альфой верхнего.
            CompositeEffect cut;
            cut.mode(CanvasComposite::DestinationIn);
            cut.sources().append(glow(static_cast<float>(rimDiffuse.get()), 2.0f));
            cut.sources().append(CompositionEffectSourceParameter {u"Ring"});
            auto const rimBrush = compositor.createEffectFactory(cut).createBrush();
            rimBrush.setSourceParameter(u"Ring", frame);

            for (auto const& face : faces) {
                face.brush(faceBrush);
            }
            for (auto const& rim : rims) {
                rim.brush(rimBrush);
            }
        });
    }

    void relight() noexcept {
        lab::attempt(lightStatus, u"Прожекторы", [this] {
            aim(faceLight, faceHeight.get(), faceRadius.get(), faceIntensity.get());
            aim(rimLight, rimHeight.get(), rimRadius.get(), rimIntensity.get());
        });
    }

    // Прожектор светит отвесно вниз с высоты z; радиус пятна задаёт внешний конус.
    void aim(SpotLight const& lamp, double z, double radius, double power) {
        Color const color = lightColor.get();
        lamp.innerConeColor(color);
        lamp.outerConeColor(color);
        // Сила света идёт к новому значению от того, что есть: один ключевой кадр в конце.
        auto const fade = compositor.createScalarKeyFrameAnimation();
        fade.insertKeyFrame(1.0f, shining ? static_cast<float>(power) : 0.0f);
        fade.duration(std::chrono::milliseconds {180});
        lamp.startAnimation(u"InnerConeIntensity", fade);
        lamp.startAnimation(u"OuterConeIntensity", fade);
        lamp.innerConeAngle(0.0f);
        lamp.outerConeAngle(static_cast<float>(std::atan2(radius, z)));
        lamp.constantAttenuation(static_cast<float>(constantAttenuation.get()));
        lamp.linearAttenuation(static_cast<float>(linearAttenuation.get()));

        if (!pointer) {
            return;
        }
        lamp.stopAnimation(u"Offset");
        auto const follow = compositor.createExpressionAnimation(u"pointer.Position + Vector3(0, 0, z)");
        follow.setReferenceParameter(u"pointer", *pointer);
        follow.setScalarParameter(u"z", static_cast<float>(z));
        lamp.startAnimation(u"Offset", follow);
    }
};

}  // namespace

lab::Experiment lab::revealPage() {
    auto const model = std::make_shared<Reveal>();

    static constexpr char16_t const* labels[] = {
        u"7", u"8", u"9", u"÷", u"4", u"5", u"6", u"×", u"1", u"2", u"3", u"−", u"0", u",", u"=", u"+",
    };

    auto pad = Grid {
        width = 440,
        height = 440,
        hAlign.center,
        vAlign.center,
        requestedTheme = ElementTheme::Dark,
        background = SolidColorBrush {color = rgb(30, 31, 34)},
        CornerRadius {12},
        Padding {10},
        rowDefinitions = u"*,*,*,*",
        columnDefinitions = u"*,*,*,*",
        onLoaded = [model](Grid const& self) { model->connect(self); },
        onPointerEntered = [model](Grid const&) { model->shine(true); },
        onPointerExited = [model](Grid const&) { model->shine(false); },
    };

    for (int index = 0; index < 16; ++index) {
        pad.children().append(Button {
            row = index / 4,
            column = index % 4,
            hAlign.stretch,
            vAlign.stretch,
            Margin {5},
            fontSize = 22,
            content = labels[index],
            onClick = [model] { model->flash(); },
            [model](Button const& self) { model->attachKey(self); },
        });
    }

    auto settings = lab::settingsPanel({
        lab::noteRow(u"Свет виден, пока указатель над клавиатурой. Щелчок по клавише -- вспышка пятна."),
        lab::statusRow(model->brushStatus),
        lab::statusRow(model->lightStatus),
        lab::group(u"Пятно на клавише", true,
                   {
                       lab::sliderRow(u"Высота прожектора", model->faceHeight, 10.0, 600.0, 1.0),
                       lab::sliderRow(u"Радиус пятна", model->faceRadius, 5.0, 400.0, 1.0),
                       lab::sliderRow(u"Сила света", model->faceIntensity, 0.0, 6.0, 0.01),
                       lab::sliderRow(u"DiffuseAmount", model->faceDiffuse, 0.0, 3.0, 0.01),
                   }),
        lab::group(u"Свет на рамках", true,
                   {
                       lab::sliderRow(u"Высота прожектора", model->rimHeight, 10.0, 600.0, 1.0),
                       lab::sliderRow(u"Радиус пятна", model->rimRadius, 5.0, 600.0, 1.0),
                       lab::sliderRow(u"Сила света", model->rimIntensity, 0.0, 6.0, 0.01),
                       lab::sliderRow(u"DiffuseAmount", model->rimDiffuse, 0.0, 3.0, 0.01),
                       lab::sliderRow(u"Толщина рамки", model->rimThickness, 0.5, 8.0, 0.5),
                       lab::sliderRow(u"Скругление", model->corner, 0.0, 24.0, 0.5),
                   }),
        lab::group(u"Оба прожектора", false,
                   {
                       lab::colorRow(u"Цвет", model->lightColor),
                       lab::sliderRow(u"ConstantAttenuation", model->constantAttenuation, 0.0, 4.0, 0.01),
                       lab::sliderRow(u"LinearAttenuation", model->linearAttenuation, 0.0, 0.05, 0.0005),
                   }),
    });

    return {pad, settings, model};
}
