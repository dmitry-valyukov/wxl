// Опыт «Образец ThumbnailLighting».
//
// Повтор образца Microsoft (WindowsAppSDK-Samples, SceneGraph, ThumbnailLighting)
// на том же API: те же графы эффекта, те же источники света, корень света --
// визуал элемента списка, плитки -- спрайты, вставшие визуалами-детьми
// элементов внутри него. Карты нормалей взяты из образца и положены в
// поверхность своего рисования, как это делает его загрузчик. Рядом для
// сравнения стоят карты, которые считает стенд: при тех же свете и графе
// видно, чья карта отвечает на свет неверно.

#include "Lab.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr int tileCount = lab::keyColumns * lab::keyRows;

constexpr char16_t const* modes[] = {
    u"PointDiffuse",
    u"SpotLightDiffuse",
    u"DistantDiffuse",
};

constexpr char16_t const* mapNames[] = {
    u"как в образце",
    u"BeveledEdges.jpg",
    u"SphericalWithMask.png",
    u"bubblenm.jpg",
    u"фаска стенда",
    u"сфера стенда",
};

// Отступления от образца по одному: от чего зависит ответ карты на свет.
constexpr char16_t const* variants[] = {
    u"как в образце",
    u"плитка 300×150",
    u"плитка 100×100",
    u"граф: один SceneLightingEffect",
    u"BorderMode Hard",
    u"спрайт повёрнут на 30°",
    u"карта без растяжения",
    u"BorderMode Hard и скруглённая обрезка",
    u"скруглённая обрезка, граница мягкая",
};

struct Sample {
    core::observable<int> mode {2};
    core::observable<int> map {1};
    core::observable<int> variant {0};

    // Далёкий свет: где стоит лампа. 0 -- справа, 90 -- сверху экрана.
    core::observable<double> azimuth {0.0};
    core::observable<double> elevation {30.0};

    // Точечный свет и прожектор: место лампы в координатах списка.
    core::observable<double> lightX {lab::stageWidth / 2.0};
    core::observable<double> lightY {lab::stageHeight / 2.0};
    core::observable<double> lightZ {75.0};

    core::observable<hstring> mapStatus;
    core::observable<hstring> brushStatus;
    core::observable<hstring> lightStatus;

    Compositor compositor;
    PointLight point;
    SpotLight spot;
    DistantLight distant;
    // «Картинка» плитки: ровный серый, к которому граф образца прибавляет свет.
    DrawingSurface image;
    DrawingSurface edgeMap;
    DrawingSurface sphereMap;
    DrawingSurface bubbleMap;
    DrawingSurface standBevel;
    DrawingSurface standSphere;
    std::vector<SpriteVisual> sprites;
    core::nullable<Visual> lightRoot;

    Sample()
        : compositor(CompositionTarget::getCompositorForCurrentThread()),
          point(compositor.createPointLight()),
          spot(compositor.createSpotLight()),
          distant(compositor.createDistantLight()),
          image(compositor, SizeInt32 {8, 8}),
          edgeMap(compositor, SizeInt32 {0, 0}),
          sphereMap(compositor, SizeInt32 {0, 0}),
          bubbleMap(compositor, SizeInt32 {0, 0}),
          standBevel(compositor, SizeInt32 {512, 512}),
          standSphere(compositor, SizeInt32 {512, 512}) {
        lab::attempt(mapStatus, u"Карты образца из Assets/NormalMaps", [this] {
            lab::loadImage(edgeMap, L"BeveledEdges.jpg");
            lab::loadImage(sphereMap, L"SphericalWithMask.png");
            lab::loadImage(bubbleMap, L"bubblenm.jpg");

            lab::drawSolid(image, rgb(96, 96, 96));

            // Фаска стенда повторяет BeveledEdges: прямые углы, кромка в 5 % стороны,
            // крутизна около 60 градусов; зелёный канал растёт вверх экрана.
            lab::ReliefShape bevel;
            bevel.corner = 0.0;
            bevel.shoulder = 0.05;
            bevel.depth = 1.8;
            bevel.yUp = true;
            lab::drawNormalMap(standBevel, lab::Relief::bevel, bevel);

            lab::ReliefShape round;
            round.yUp = true;
            lab::drawNormalMap(standSphere, lab::Relief::sphere, round);
        });

        lab::onAny([this] { paint(); }, mode, map, variant);
        lab::onAny([this] { relight(); }, mode, azimuth, elevation, lightX, lightY, lightZ);
    }

    // Вызывается из скобок плитки, до того как она встала в список.
    void attachTile(Border const& tile) {
        auto const sprite = compositor.createSpriteVisual();
        sprite.relativeSizeAdjustment({1.0f, 1.0f});
        ElementCompositionPreview::setElementChildVisual(tile, sprite);
        sprites.push_back(sprite);
    }

    // Список на экране: его визуал -- и пространство координат света, и цель,
    // как в образце.
    void connect(Grid const& list) {
        lightRoot = ElementCompositionPreview::getElementVisual(list);
        paint();
        relight();
    }

    DrawingSurface const& chosenMap() const {
        switch (map.get()) {
        case 1: return edgeMap;
        case 2: return sphereMap;
        case 3: return bubbleMap;
        case 4: return standBevel;
        case 5: return standSphere;
        // В образце: кромки у Point и Spot Diffuse, сфера у далёкого света.
        default: return mode.get() == 2 ? sphereMap : edgeMap;
        }
    }

    void paint() noexcept {
        lab::attempt(brushStatus, modes[std::clamp(mode.get(), 0, 2)], [this] {
            auto const source = [](char16_t const* name) {
                return GraphicsEffectSource {CompositionEffectSourceParameter {name}};
            };

            // Result = Image + DiffuseAmount * Diffuse color -- так подписан граф в образце.
            auto const lit = [&](float diffuse) {
                SceneLightingEffect light;
                light.ambientAmount(0.0f);
                light.diffuseAmount(diffuse);
                light.specularAmount(0.0f);
                light.normalMapSource(source(u"NormalMap"));

                CompositeEffect sum;
                sum.mode(CanvasComposite::Add);
                sum.sources().append(source(u"ImageSource"));
                sum.sources().append(light);
                return sum;
            };

            CompositionEffectFactory factory = [&] {
                if (variant.get() == 3) {
                    SceneLightingEffect light;
                    light.ambientAmount(0.0f);
                    light.diffuseAmount(1.0f);
                    light.specularAmount(0.0f);
                    light.normalMapSource(source(u"NormalMap"));
                    return compositor.createEffectFactory(light);
                }
                if (mode.get() != 2) {
                    return compositor.createEffectFactory(lit(0.75f));
                }
                // У далёкого света образец ещё вырезает плитку по альфе карты.
                CompositeEffect cut;
                cut.mode(CanvasComposite::DestinationIn);
                cut.sources().append(lit(0.5f));
                cut.sources().append(source(u"NormalMap"));
                return compositor.createEffectFactory(cut);
            }();

            auto const picture = image.brush();
            picture.stretch(CompositionStretch::Fill);

            for (auto const& sprite : sprites) {
                auto const normals = chosenMap().brush();
                normals.stretch(variant.get() == 6 ? CompositionStretch::None : CompositionStretch::Fill);

                auto const brush = factory.createBrush();
                if (variant.get() != 3) {
                    brush.setSourceParameter(u"ImageSource", picture);
                }
                brush.setSourceParameter(u"NormalMap", normals);
                sprite.brush(brush);

                sprite.relativeSizeAdjustment({1.0f, 1.0f});
                sprite.size({0.0f, 0.0f});
                sprite.rotationAngleInDegrees(0.0f);
                sprite.borderMode(CompositionBorderMode::Inherit);
                switch (variant.get()) {
                case 1:
                    sprite.relativeSizeAdjustment({0.0f, 0.0f});
                    sprite.size({300.0f, 150.0f});
                    break;
                case 2:
                    sprite.relativeSizeAdjustment({0.0f, 0.0f});
                    sprite.size({100.0f, 100.0f});
                    break;
                case 4:
                    sprite.borderMode(CompositionBorderMode::Hard);
                    break;
                case 5:
                    sprite.centerPoint({lab::keySide / 2.0f, lab::keySide / 2.0f, 0.0f});
                    sprite.rotationAngleInDegrees(30.0f);
                    break;
                case 7:
                case 8: {
                    // Обрезку обёртка снять не может: эти два отступления смотрят последними.
                    auto const outline = compositor.createRoundedRectangleGeometry();
                    outline.size({lab::keySide, lab::keySide});
                    outline.cornerRadius({24.0f, 24.0f});
                    sprite.clip(compositor.createGeometricClip(outline));
                    if (variant.get() == 7) {
                        sprite.borderMode(CompositionBorderMode::Hard);
                    }
                    break;
                }
                default:
                    break;
                }
            }
        });
    }

    void relight() noexcept {
        if (!lightRoot) {
            return;
        }
        lab::attempt(lightStatus, u"Источник света", [this] {
            Visual const& root = *lightRoot;
            point.targets().removeAll();
            spot.targets().removeAll();
            distant.targets().removeAll();

            Vector3 const place {static_cast<float>(lightX.get()), static_cast<float>(lightY.get()),
                                 static_cast<float>(lightZ.get())};

            switch (mode.get()) {
            case 0:
                point.coordinateSpace(root);
                point.targets().add(root);
                point.offset(place);
                break;
            case 1:
                spot.coordinateSpace(root);
                spot.targets().add(root);
                spot.innerConeAngle(3.14159265f / 4.0f);
                spot.outerConeAngle(3.14159265f / 3.5f);
                spot.direction({0.0f, 0.0f, -1.0f});
                spot.offset(place);
                break;
            default: {
                // Свет идёт от лампы: обратно её направлению. Ось y визуала смотрит вниз.
                double const a = azimuth.get() * 3.14159265358979 / 180.0;
                double const e = elevation.get() * 3.14159265358979 / 180.0;
                distant.coordinateSpace(root);
                distant.targets().add(root);
                distant.direction({static_cast<float>(-std::cos(a) * std::cos(e)),
                                   static_cast<float>(std::sin(a) * std::cos(e)), static_cast<float>(-std::sin(e))});
                break;
            }
            }
        });
    }
};

}  // namespace

lab::Experiment lab::samplePage() {
    auto const model = std::make_shared<Sample>();

    auto list = Grid {
        width = lab::stageWidth,
        height = lab::stageHeight,
        hAlign.center,
        vAlign.center,
        background = SolidColorBrush {color = rgb(30, 31, 34)},
        rowDefinitions = u"*,*",
        columnDefinitions = u"*,*,*",
        onLoaded = [model](Grid const& self) { model->connect(self); },
    };

    for (int index = 0; index < tileCount; ++index) {
        list.children().append(Border {
            row = index / lab::keyColumns,
            column = index % lab::keyColumns,
            width = lab::keySide,
            height = lab::keySide,
            hAlign.center,
            vAlign.center,
            [model](Border const& self) { model->attachTile(self); },
        });
    }

    auto settings = lab::settingsPanel({
        lab::noteRow(u"Повтор образца Microsoft ThumbnailLighting: его графы, его источники света с корнем на "
                     u"визуале списка, его карты нормалей. Далёкий свет справа обязан осветить правую кромку "
                     u"плитки и оставить верхнюю и нижнюю равными."),
        lab::statusRow(model->mapStatus),
        lab::statusRow(model->brushStatus),
        lab::statusRow(model->lightStatus),
        lab::group(u"Образец", true,
                   {
                       lab::choiceRow(u"Режим образца", model->mode, modes),
                       lab::choiceRow(u"Карта нормалей", model->map, mapNames),
                       lab::choiceRow(u"Отступление от образца", model->variant, variants),
                   }),
        lab::group(u"Далёкий свет", true,
                   {
                       lab::sliderRow(u"Азимут лампы", model->azimuth, 0.0, 360.0, 1.0),
                       lab::sliderRow(u"Высота лампы", model->elevation, 1.0, 90.0, 1.0),
                   }),
        lab::group(u"Точечный свет и прожектор", true,
                   {
                       lab::sliderRow(u"Лампа: x", model->lightX, 0.0, lab::stageWidth, 1.0),
                       lab::sliderRow(u"Лампа: y", model->lightY, 0.0, lab::stageHeight, 1.0),
                       lab::sliderRow(u"Лампа: z", model->lightZ, 1.0, 400.0, 1.0),
                   }),
    });

    return {list, settings, model};
}
