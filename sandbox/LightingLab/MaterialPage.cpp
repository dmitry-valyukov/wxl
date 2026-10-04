// Опыт «Свет композитора: материал».
//
// Шесть клавиш -- визуалы композитора XAML этого потока, каждая закрашена
// кистью эффекта с SceneLightingEffect и своей картой нормалей: подушка,
// фаска, чаша, купол, сфера, плоская. Под ними панель с плоской нормалью: на
// ней видно само пятно света. Свет -- CompositionLight, нацеленный на корень
// сцены; место лампы считает композитор -- выражением от набора свойств
// указателя или ключевыми кадрами.

#include "Lab.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr int tileCount = lab::keyColumns * lab::keyRows;

constexpr lab::Relief reliefs[tileCount] = {
    lab::Relief::cushion, lab::Relief::bevel, lab::Relief::dish,
    lab::Relief::dome,    lab::Relief::sphere, lab::Relief::flat,
};

// Карта вдвое плотнее клавиши: на масштабе 200 % пиксель в пиксель.
constexpr int mapPixels = 400;

constexpr char16_t const* recipes[] = {
    u"Цвет × свет",
    u"Цвет + свет",
    u"Цвет × рассеянный + блик",
    u"Только свет",
    u"Карта нормалей как цвет",
    // Свет не от CompositionLight, а от эффекта освещения Win2D в самом графе:
    // лампа записана в его свойствах, вход -- карта нормалей.
    u"Win2D PointDiffuseEffect",
    u"Win2D DistantDiffuseEffect",
    u"Win2D SpotDiffuseEffect",
    u"Win2D PointSpecularEffect",
    u"Win2D DistantSpecularEffect",
    u"Win2D SpotSpecularEffect",
};
constexpr char16_t const* reflectanceModels[] = {u"BlinnPhong", u"PhysicallyBasedBlinnPhong"};
constexpr char16_t const* lightKinds[] = {u"PointLight", u"SpotLight", u"DistantLight"};
constexpr char16_t const* motions[] = {u"За указателем (выражение)", u"По кругу (ключевые кадры)", u"В центре"};
constexpr char16_t const* shapings[] = {
    u"Маска в графе, визуал без обрезки",
    u"Обрезка CompositionGeometricClip по форме",
    u"Маска в графе, обрезка описанным кругом",
    u"Без формы и без обрезки",
};

struct Material {
    // Материал.
    core::observable<int> recipe {0};
    core::observable<double> ambientAmount {0.6};
    core::observable<double> diffuseAmount {1.0};
    core::observable<double> specularAmount {0.5};
    core::observable<double> specularShine {40.0};
    core::observable<int> reflectance {0};
    core::observable<bool> animated {false};
    core::observable<Color> keyColor {rgb(68, 72, 79)};
    core::observable<Color> panelColor {rgb(42, 44, 49)};

    // Рельеф.
    core::observable<double> corner {0.12};
    core::observable<double> shoulder {0.16};
    core::observable<double> depth {1.0};
    core::observable<bool> yUp {true};
    // Чем клавише дана её форма. Обрезка визуала скруглённым прямоугольником
    // поворачивает ответ карты нормалей на свет (у круга -- нет), поэтому по
    // умолчанию форма -- маска в самом графе, а обрезка оставлена для сравнения.
    core::observable<int> shaping {0};

    // Лампа.
    core::observable<int> lightKind {0};
    core::observable<int> motion {0};
    core::observable<Color> lightColor {rgb(255, 246, 232)};
    core::observable<double> intensity {1.0};
    core::observable<double> height {140.0};
    core::observable<double> constantAttenuation {1.0};
    core::observable<double> linearAttenuation {0.0};
    core::observable<double> quadraticAttenuation {0.0};
    core::observable<double> innerCone {20.0};
    core::observable<double> outerCone {45.0};
    core::observable<double> azimuth {120.0};
    core::observable<double> elevation {50.0};

    // Свет со всех сторон.
    core::observable<bool> ambientOn {true};
    core::observable<Color> ambientColor {rgb(140, 146, 168)};
    core::observable<double> ambientIntensity {1.0};

    core::observable<hstring> brushStatus;
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
    std::vector<SpriteVisual> tiles;
    std::vector<CompositionRoundedRectangleGeometry> outlines;
    // По карте на клавишу и последняя, плоская, -- панели.
    std::vector<DrawingSurface> maps;
    // Маски формы: скруглённый квадрат клавиши и круг сферы.
    DrawingSurface squareShape;
    DrawingSurface roundShape;
    core::nullable<CompositionPropertySet> pointer;

    Material()
        : compositor(CompositionTarget::getCompositorForCurrentThread()),
          root(compositor.createContainerVisual()),
          panel(compositor.createSpriteVisual()),
          keyBrush(compositor.createColorBrush(keyColor.get())),
          panelBrush(compositor.createColorBrush(panelColor.get())),
          ambient(compositor.createAmbientLight()),
          point(compositor.createPointLight()),
          spot(compositor.createSpotLight()),
          distant(compositor.createDistantLight()),
          squareShape(compositor, SizeInt32 {mapPixels, mapPixels}),
          roundShape(compositor, SizeInt32 {mapPixels, mapPixels}) {
        root.size({lab::stageWidth, lab::stageHeight});
        panel.relativeSizeAdjustment({1.0f, 1.0f});
        root.children().insertAtBottom(panel);

        for (int index = 0; index < tileCount; ++index) {
            maps.emplace_back(compositor, SizeInt32 {mapPixels, mapPixels});
        }
        maps.emplace_back(compositor, SizeInt32 {8, 8});
        makeTiles();

        point.coordinateSpace(root);
        spot.coordinateSpace(root);
        distant.coordinateSpace(root);
        spot.direction({0.0f, 0.0f, -1.0f});

        keyColor.on_change([this](Color const& value) noexcept { keyBrush.color(value); });
        panelColor.on_change([this](Color const& value) noexcept { panelBrush.color(value); });
        lab::onAny([this] { reshape(); }, corner, shoulder, depth, yUp);
        lab::onAny([this] { paint(); }, recipe, ambientAmount, diffuseAmount, specularAmount, specularShine,
                   reflectance, animated);
        lab::onAny(
            [this] {
                makeTiles();
                reshape();
                paint();
            },
            shaping);
        lab::onAny([this] { relight(); }, recipe);
        // У графов Win2D лампа записана в самом эффекте: её настройки меняют граф.
        lab::onAny(
            [this] {
                if (recipe.get() >= 5) {
                    paint();
                }
            },
            height, lightColor, azimuth, elevation, outerCone);
        lab::onAny([this] { relight(); }, lightKind, motion, lightColor, intensity, height, constantAttenuation,
                   linearAttenuation, quadraticAttenuation, innerCone, outerCone, azimuth, elevation, ambientOn,
                   ambientColor, ambientIntensity);

        reshape();
        paint();
        relight();
    }

    // Сцена встаёт визуалом-ребёнком элемента; указатель над элементом ведёт лампу.
    void attach(Border const& host) {
        ElementCompositionPreview::setElementChildVisual(host, root);
        pointer = ElementCompositionPreview::getPointerPositionPropertySet(host);
        relight();
    }

    // Клавиши заводятся заново на каждую смену способа дать им форму: обрезку,
    // раз поставленную визуалу, обёртка снять не может.
    void makeTiles() noexcept {
        lab::attempt(brushStatus, u"Визуалы клавиш", [this] {
            for (auto const& tile : tiles) {
                root.children().remove(tile);
            }
            tiles.clear();
            outlines.clear();

            for (int index = 0; index < tileCount; ++index) {
                auto const tile = compositor.createSpriteVisual();
                tile.size({lab::keySide, lab::keySide});
                tile.offset({lab::keyLeft(index % lab::keyColumns), lab::keyTop(index / lab::keyColumns), 0.0f});

                auto const outline = compositor.createRoundedRectangleGeometry();
                if (shaping.get() == 1) {
                    outline.size({lab::keySide, lab::keySide});
                    tile.clip(compositor.createGeometricClip(outline));
                } else if (shaping.get() == 2) {
                    // Круг, описанный вокруг клавиши: он ничего от неё не срезает.
                    float const across = lab::keySide * 1.4143f;
                    float const outside = (across - lab::keySide) / 2.0f;
                    outline.size({across, across});
                    outline.offset({-outside, -outside});
                    outline.cornerRadius({across / 2.0f, across / 2.0f});
                    tile.clip(compositor.createGeometricClip(outline));
                }

                root.children().insertAtTop(tile);
                tiles.push_back(tile);
                outlines.push_back(outline);
            }
        });
    }

    void reshape() noexcept {
        lab::attempt(brushStatus, u"Карты нормалей", [this] {
            lab::ReliefShape const shape {corner.get(), shoulder.get(), depth.get(), yUp.get()};
            for (int index = 0; index < tileCount; ++index) {
                lab::drawNormalMap(maps[index], reliefs[index], shape);
                if (shaping.get() == 1) {
                    float const radius = reliefs[index] == lab::Relief::sphere
                                             ? lab::keySide / 2.0f
                                             : static_cast<float>(corner.get()) * lab::keySide;
                    outlines[index].cornerRadius({radius, radius});
                }
            }
            lab::drawNormalMap(maps[tileCount], lab::Relief::flat, shape);
            lab::drawShape(squareShape, static_cast<float>(corner.get()) * mapPixels);
            lab::drawShape(roundShape, mapPixels / 2.0f);
        });
    }

    // Граф эффекта собирается заново на каждое изменение: доли света в нём --
    // постоянные описания. При animated они объявлены анимируемыми свойствами
    // кисти, в описании стоят нули, а настоящие значения пишутся в набор
    // свойств кисти: если клавиши освещены, свойства работают.
    void paint() noexcept {
        lab::attempt(brushStatus, recipes[std::clamp(recipe.get(), 0, 10)], [this] {
            bool const viaProperties = animated.get();
            float const ambientShare = static_cast<float>(ambientAmount.get());
            float const diffuseShare = static_cast<float>(diffuseAmount.get());
            float const specularShare = static_cast<float>(specularAmount.get());
            float const shine = static_cast<float>(specularShine.get());

            std::vector<hstring> animatedNames;
            std::vector<std::pair<hstring, float>> animatedValues;

            auto const source = [](char16_t const* name) {
                return GraphicsEffectSource {CompositionEffectSourceParameter {name}};
            };

            auto const light = [&](std::u16string const& name, float ambientPart, float diffusePart,
                                   float specularPart) {
                SceneLightingEffect effect;
                effect.name(hstring {name});
                effect.reflectanceModel(reflectance.get() == 0
                                            ? SceneLightingEffectReflectanceModel::BlinnPhong
                                            : SceneLightingEffectReflectanceModel::PhysicallyBasedBlinnPhong);
                effect.normalMapSource(source(u"NormalMap"));

                std::pair<char16_t const*, float> const shares[] = {
                    {u".AmbientAmount", ambientPart},
                    {u".DiffuseAmount", diffusePart},
                    {u".SpecularAmount", specularPart},
                    {u".SpecularShine", shine},
                };
                for (auto const& [property, value] : shares) {
                    if (viaProperties) {
                        animatedNames.emplace_back(name + property);
                        animatedValues.emplace_back(hstring {name + property}, value);
                    }
                }
                effect.ambientAmount(viaProperties ? 0.0f : ambientPart);
                effect.diffuseAmount(viaProperties ? 0.0f : diffusePart);
                effect.specularAmount(viaProperties ? 0.0f : specularPart);
                effect.specularShine(viaProperties ? 1.0f : shine);
                return effect;
            };

            // Произведение двух входов: свет, упавший на цвет.
            auto const product = [](GraphicsEffectSource const& first, GraphicsEffectSource const& second) {
                ArithmeticCompositeEffect effect;
                effect.source1(first);
                effect.source2(second);
                effect.multiplyAmount(1.0f);
                effect.source1Amount(0.0f);
                effect.source2Amount(0.0f);
                return effect;
            };

            // Эффект освещения Win2D: лампа над серединой клавиши, вход -- карта
            // нормалей. Вид: 0 -- точечная, 1 -- далёкая, 2 -- прожектор.
            auto const lamp = [&](int kind, bool glossy) -> Object {
                float const middle = lab::keySide / 2.0f;
                Vector3 const above {middle, middle, static_cast<float>(height.get())};
                Color const color = lightColor.get();
                float const turn = static_cast<float>(azimuth.get() * 3.14159265358979 / 180.0);
                float const lift = static_cast<float>(elevation.get() * 3.14159265358979 / 180.0);
                float const cone = static_cast<float>(outerCone.get() * 3.14159265358979 / 180.0);
                auto const map = source(u"NormalMap");

                if (kind == 0 && !glossy) {
                    PointDiffuseEffect effect;
                    effect.source(map);
                    effect.lightPosition(above);
                    effect.lightColor(color);
                    effect.diffuseAmount(diffuseShare);
                    return effect;
                }
                if (kind == 1 && !glossy) {
                    DistantDiffuseEffect effect;
                    effect.source(map);
                    effect.azimuth(turn);
                    effect.elevation(lift);
                    effect.lightColor(color);
                    effect.diffuseAmount(diffuseShare);
                    return effect;
                }
                if (kind == 2 && !glossy) {
                    SpotDiffuseEffect effect;
                    effect.source(map);
                    effect.lightPosition(above);
                    effect.lightTarget({middle, middle, 0.0f});
                    effect.limitingConeAngle(cone);
                    effect.lightColor(color);
                    effect.diffuseAmount(diffuseShare);
                    return effect;
                }
                if (kind == 0) {
                    PointSpecularEffect effect;
                    effect.source(map);
                    effect.lightPosition(above);
                    effect.lightColor(color);
                    effect.specularAmount(specularShare);
                    effect.specularExponent(shine);
                    return effect;
                }
                if (kind == 1) {
                    DistantSpecularEffect effect;
                    effect.source(map);
                    effect.azimuth(turn);
                    effect.elevation(lift);
                    effect.lightColor(color);
                    effect.specularAmount(specularShare);
                    effect.specularExponent(shine);
                    return effect;
                }
                SpotSpecularEffect effect;
                effect.source(map);
                effect.lightPosition(above);
                effect.lightTarget({middle, middle, 0.0f});
                effect.limitingConeAngle(cone);
                effect.lightColor(color);
                effect.specularAmount(specularShare);
                effect.specularExponent(shine);
                return effect;
            };

            int const chosen = recipe.get();
            auto const lit = [&]() -> Object {
                switch (chosen) {
                case 0:
                    return product(source(u"Base"), light(u"Light", ambientShare, diffuseShare, specularShare));
                case 1: {
                    CompositeEffect sum;
                    sum.mode(CanvasComposite::Add);
                    sum.sources().append(source(u"Base"));
                    sum.sources().append(light(u"Light", ambientShare, diffuseShare, specularShare));
                    return sum;
                }
                case 2: {
                    // Блик не красится цветом клавиши: он складывается поверх.
                    ArithmeticCompositeEffect sum;
                    sum.source1(product(source(u"Base"), light(u"Diffuse", ambientShare, diffuseShare, 0.0f)));
                    sum.source2(light(u"Specular", 0.0f, 0.0f, specularShare));
                    sum.multiplyAmount(0.0f);
                    sum.source1Amount(1.0f);
                    sum.source2Amount(1.0f);
                    return sum;
                }
                case 3:
                    return light(u"Light", ambientShare, diffuseShare, specularShare);
                // Эффект Win2D один, без цвета: фабрика отвечает именно про него.
                case 5:
                case 6:
                case 7:
                    return lamp(chosen - 5, false);
                case 8:
                case 9:
                case 10:
                    return lamp(chosen - 8, true);
                default: {
                    // Сама карта: что в ней записано, то и на экране.
                    ArithmeticCompositeEffect plain;
                    plain.source1(source(u"NormalMap"));
                    plain.source2(source(u"NormalMap"));
                    plain.multiplyAmount(0.0f);
                    plain.source1Amount(1.0f);
                    plain.source2Amount(0.0f);
                    return plain;
                }
                }
            };

            // Граф клавиши и граф панели: у клавиши при маске поверх света стоит
            // вырезание по альфе формы -- от нижнего входа остаётся то, что под
            // альфой верхнего.
            auto const compile = [&](bool masked) {
                animatedNames.clear();
                animatedValues.clear();
                Object const whole = lit();
                if (!masked) {
                    return viaProperties ? compositor.createEffectFactory(whole, animatedNames)
                                         : compositor.createEffectFactory(whole);
                }
                CompositeEffect cut;
                cut.mode(CanvasComposite::DestinationIn);
                cut.sources().append(whole);
                cut.sources().append(source(u"Shape"));
                return viaProperties ? compositor.createEffectFactory(cut, animatedNames)
                                     : compositor.createEffectFactory(cut);
            };

            bool const masked = shaping.get() == 0 || shaping.get() == 2;
            auto const keyFactory = compile(masked);
            auto const panelFactory = masked ? compile(false) : keyFactory;

            for (int index = 0; index <= tileCount; ++index) {
                bool const key = index < tileCount;
                auto const brush = (key ? keyFactory : panelFactory).createBrush();
                auto const map = maps[index].brush();
                map.stretch(CompositionStretch::Fill);
                brush.setSourceParameter(u"NormalMap", map);
                if (chosen < 3) {
                    brush.setSourceParameter(u"Base", key ? keyBrush : panelBrush);
                }
                if (key && masked) {
                    auto const shape = (reliefs[index] == lab::Relief::sphere ? roundShape : squareShape).brush();
                    shape.stretch(CompositionStretch::Fill);
                    brush.setSourceParameter(u"Shape", shape);
                }
                for (auto const& [name, value] : animatedValues) {
                    brush.properties().insertScalar(name, value);
                }
                (key ? tiles[index] : panel).brush(brush);
            }
        });
    }

    // Невключённый источник целей не имеет: на визуал разрешено не больше двух
    // источников помимо общего света.
    void relight() noexcept {
        lab::attempt(lightStatus, u"Источник света", [this] {
            ambient.targets().removeAll();
            point.targets().removeAll();
            spot.targets().removeAll();
            distant.targets().removeAll();

            // Карта как цвет и графы с лампой внутри эффекта Win2D показываются без
            // источников: визуал, на который не нацелен ни один, рисуется как есть.
            if (recipe.get() >= 4) {
                return;
            }

            ambient.color(ambientColor.get());
            ambient.intensity(static_cast<float>(ambientIntensity.get()));
            if (ambientOn.get()) {
                ambient.targets().add(root);
            }

            Color const color = lightColor.get();
            float const power = static_cast<float>(intensity.get());

            switch (lightKind.get()) {
            case 0:
                point.color(color);
                point.intensity(power);
                point.constantAttenuation(static_cast<float>(constantAttenuation.get()));
                point.linearAttenuation(static_cast<float>(linearAttenuation.get()));
                point.quadraticAttenuation(static_cast<float>(quadraticAttenuation.get()));
                move(point);
                point.targets().add(root);
                break;
            case 1:
                spot.innerConeColor(color);
                spot.outerConeColor(color);
                spot.innerConeIntensity(power);
                spot.outerConeIntensity(power);
                spot.innerConeAngleInDegrees(static_cast<float>(innerCone.get()));
                spot.outerConeAngleInDegrees(static_cast<float>(std::max(outerCone.get(), innerCone.get())));
                spot.constantAttenuation(static_cast<float>(constantAttenuation.get()));
                spot.linearAttenuation(static_cast<float>(linearAttenuation.get()));
                spot.quadraticAttenuation(static_cast<float>(quadraticAttenuation.get()));
                move(spot);
                spot.targets().add(root);
                break;
            default: {
                // Лампа стоит под азимутом и высотой, как у relief_helper: 0 --
                // справа, 90 -- сверху экрана. Свет идёт от неё, то есть обратно.
                double const a = azimuth.get() * 3.14159265358979 / 180.0;
                double const e = elevation.get() * 3.14159265358979 / 180.0;
                distant.color(color);
                distant.intensity(power);
                distant.direction({static_cast<float>(-std::cos(a) * std::cos(e)),
                                   static_cast<float>(std::sin(a) * std::cos(e)), static_cast<float>(-std::sin(e))});
                distant.targets().add(root);
                break;
            }
            }
        });
    }

    template <class Light>
    void move(Light const& lamp) {
        lamp.stopAnimation(u"Offset");
        float const z = static_cast<float>(height.get());
        float const centreX = lab::stageWidth / 2.0f, centreY = lab::stageHeight / 2.0f;

        if (motion.get() == 0 && pointer) {
            auto const follow = compositor.createExpressionAnimation(u"pointer.Position + Vector3(0, 0, z)");
            follow.setReferenceParameter(u"pointer", *pointer);
            follow.setScalarParameter(u"z", z);
            lamp.startAnimation(u"Offset", follow);
            return;
        }
        if (motion.get() == 1) {
            auto const orbit = compositor.createVector3KeyFrameAnimation();
            auto const steady = compositor.createLinearEasingFunction();
            constexpr int steps = 16;
            for (int step = 0; step <= steps; ++step) {
                double const turn = 2.0 * 3.14159265358979 * step / steps;
                orbit.insertKeyFrame(static_cast<float>(step) / steps,
                                     {centreX + static_cast<float>(std::cos(turn)) * centreX * 0.8f,
                                      centreY + static_cast<float>(std::sin(turn)) * centreY * 0.8f, z},
                                     steady);
            }
            orbit.duration(std::chrono::seconds {8});
            orbit.iterationBehavior(AnimationIterationBehavior::Forever);
            lamp.startAnimation(u"Offset", orbit);
            return;
        }
        lamp.offset({centreX, centreY, z});
    }
};

}  // namespace

lab::Experiment lab::materialPage() {
    auto const model = std::make_shared<Material>();

    auto stage = Border {
        width = double {lab::stageWidth},
        height = double {lab::stageHeight},
        hAlign.center,
        vAlign.center,
        background = SolidColorBrush {color = rgb(24, 25, 28)},
        [model](Border const& host) { model->attach(host); },
    };

    auto settings = lab::settingsPanel({
        lab::noteRow(u"Клавиши по порядку: подушка, фаска, чаша; купол, сфера, плоская. Под ними панель с "
                     u"плоской нормалью."),
        lab::noteRow(u"Замечено прогоном: ответ карты нормалей на свет повёрнут относительно самой карты, и "
                     u"угол зависит от обрезки визуала. Способ дать клавише форму выбирается в группе «Рельеф»; "
                     u"граф «Карта нормалей как цвет» показывает саму карту."),
        lab::statusRow(model->brushStatus),
        lab::statusRow(model->lightStatus),
        lab::group(u"Материал", true,
                   {
                       lab::choiceRow(u"Граф эффекта", model->recipe, recipes),
                       lab::sliderRow(u"AmbientAmount", model->ambientAmount, 0.0, 2.0, 0.01),
                       lab::sliderRow(u"DiffuseAmount", model->diffuseAmount, 0.0, 3.0, 0.01),
                       lab::sliderRow(u"SpecularAmount", model->specularAmount, 0.0, 3.0, 0.01),
                       lab::sliderRow(u"SpecularShine", model->specularShine, 1.0, 200.0, 1.0),
                       lab::choiceRow(u"ReflectanceModel", model->reflectance, reflectanceModels),
                       lab::toggleRow(u"Доли света анимируемыми свойствами кисти", model->animated),
                       lab::colorRow(u"Цвет клавиш", model->keyColor),
                       lab::colorRow(u"Цвет панели", model->panelColor),
                   }),
        lab::group(u"Источник света", true,
                   {
                       lab::choiceRow(u"Вид", model->lightKind, lightKinds),
                       lab::choiceRow(u"Движение лампы", model->motion, motions),
                       lab::colorRow(u"Цвет", model->lightColor),
                       lab::sliderRow(u"Intensity", model->intensity, 0.0, 4.0, 0.01),
                       lab::sliderRow(u"Высота над сценой, Offset.Z", model->height, 5.0, 600.0, 1.0),
                       lab::sliderRow(u"ConstantAttenuation", model->constantAttenuation, 0.0, 4.0, 0.01),
                       lab::sliderRow(u"LinearAttenuation", model->linearAttenuation, 0.0, 0.05, 0.0005),
                       lab::sliderRow(u"QuadraticAttenuation", model->quadraticAttenuation, 0.0, 0.001, 0.00001),
                       lab::sliderRow(u"Внутренний конус, градусы (SpotLight)", model->innerCone, 0.0, 89.0, 1.0),
                       lab::sliderRow(u"Внешний конус, градусы (SpotLight)", model->outerCone, 0.0, 89.0, 1.0),
                       lab::sliderRow(u"Азимут лампы, градусы (DistantLight)", model->azimuth, 0.0, 360.0, 1.0),
                       lab::sliderRow(u"Высота лампы, градусы (DistantLight)", model->elevation, 1.0, 90.0, 1.0),
                   }),
        lab::group(u"Общий свет (AmbientLight)", false,
                   {
                       lab::toggleRow(u"Включён", model->ambientOn),
                       lab::colorRow(u"Цвет", model->ambientColor),
                       lab::sliderRow(u"Intensity", model->ambientIntensity, 0.0, 4.0, 0.01),
                   }),
        lab::group(u"Рельеф (карта нормалей)", false,
                   {
                       lab::sliderRow(u"Скругление угла, доля стороны", model->corner, 0.0, 0.5, 0.01),
                       lab::sliderRow(u"Ширина плеча, доля стороны", model->shoulder, 0.0, 0.5, 0.01),
                       lab::sliderRow(u"Крутизна (минус -- вдавлено)", model->depth, -3.0, 3.0, 0.05),
                       lab::toggleRow(u"Зелёный канал карты растёт вверх", model->yUp),
                       lab::choiceRow(u"Форма клавиши", model->shaping, shapings),
                   }),
    });

    return {stage, settings, model};
}
