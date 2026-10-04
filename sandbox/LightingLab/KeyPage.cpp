// Опыт «Клавиша на карте нормалей».
//
// Прототип света для Button3DEffect. Форма клавиши -- та же, что задают его
// параметры: тело, поднятое над панелью, с кантом по краю и лицом чашей или
// куполом (emboss). Здесь форма записана картой нормалей, а свет считает
// композитор XAML этого потока: общий свет, постоянная далёкая лампа (shadow
// делит свет между ними) и прожектор над указателем. Лицо получает пятно по
// своей форме, у канта светлеет сторона, обращённая к указателю; чёрная
// обводка -- отдельный визуал, на который лампы не нацелены.
//
// Спрайты клавиш -- с жёсткой границей: при мягкой ответ карты нормалей на
// свет повёрнут. Гладкий край формы даёт маска в графе эффекта.

#include "Lab.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr double pi = 3.14159265358979;

// Карта и маска вдвое плотнее клавиши: на масштабе 200 % пиксель в пиксель.
constexpr int density = 2;

// Форма -- размер клавиши и знак её emboss: у последней он обратный, чтобы
// чаша и купол стояли рядом.
struct Form {
    float width, height;
    double embossSign;
};
constexpr Form forms[] = {
    {84.0f, 64.0f, 1.0},
    {256.0f, 176.0f, 1.0},
    {256.0f, 176.0f, -1.0},
};
constexpr int formCount = static_cast<int>(std::size(forms));

// Вид клавиши -- форма и материал: цифровая или клавиша действия.
struct Kind {
    int form;
    bool action;
};
constexpr Kind kinds[] = {{0, false}, {0, true}, {1, false}, {2, true}};

struct Place {
    int kind;
    float left, top;
    char16_t const* label;
};

constexpr char16_t const* padLabels[] = {
    u"7", u"8", u"9", u"÷", u"4", u"5", u"6", u"×", u"1", u"2", u"3", u"−", u"0", u",", u"=", u"+",
};

// Слева клавиатура четыре на четыре, правый её столбец -- клавиши действия;
// справа две большие клавиши, на которых форму видно глазом.
std::vector<Place> layout() {
    std::vector<Place> places;
    constexpr float gap = 12.0f, padLeft = 28.0f, padTop = 99.0f;
    for (int index = 0; index < 16; ++index) {
        int const column = index % 4, row = index / 4;
        places.push_back({column == 3 ? 1 : 0, padLeft + column * (forms[0].width + gap),
                          padTop + row * (forms[0].height + gap), padLabels[index]});
    }
    places.push_back({2, 436.0f, 54.0f, u"emboss"});
    places.push_back({3, 436.0f, 260.0f, u"−emboss"});
    return places;
}

constexpr std::chrono::milliseconds fadeTime {180};

constexpr char16_t const* lampKinds[] = {u"SpotLight: прожектор отвесно вниз", u"PointLight: точечная лампа"};

struct Keys {
    // Форма.
    core::observable<double> emboss {-1.0};
    core::observable<double> rim {3.0};
    core::observable<double> slope {1.0};
    core::observable<bool> rounded {false};
    core::observable<double> corner {8.0};

    // Постоянный свет: сколько его от лампы и сколько со всех сторон.
    core::observable<double> shadow {0.7};
    core::observable<double> azimuth {120.0};
    core::observable<double> elevation {50.0};
    core::observable<Color> lampColor {rgb(255, 255, 255)};
    core::observable<Color> ambientColor {rgb(255, 255, 255)};

    // Лампа над указателем.
    core::observable<bool> pointerOn {true};
    // Прожектор гасит свет к краю конуса, точечная лампа -- только расстоянием:
    // под скользящим светом точечной кант, обращённый к ней, ярче лица.
    core::observable<int> lampKind {0};
    core::observable<double> spotSize {260.0};
    // Поперечник круга, в котором прожектор светит в полную силу: внутренний
    // конус. При нуле свет убывает от самой середины пятна.
    core::observable<double> spotCore {120.0};
    core::observable<double> spotHeight {30.0};
    core::observable<double> spotIntensity {0.4};
    core::observable<double> linearAttenuation {0.0};
    core::observable<double> quadraticAttenuation {0.0};
    core::observable<Color> spotColor {rgb(255, 255, 255)};

    // Материал.
    core::observable<double> specularAmount {0.15};
    core::observable<double> specularShine {30.0};
    core::observable<Color> numericColor {rgb(68, 72, 79)};
    core::observable<Color> actionColor {rgb(176, 98, 30)};
    core::observable<bool> hardBorder {true};

    core::observable<hstring> mapStatus;
    core::observable<hstring> brushStatus;
    core::observable<hstring> lightStatus;

    std::vector<Place> places = layout();

    Compositor compositor;
    ContainerVisual root;
    // Обводки -- под клавишами и вне света: лампы нацелены только на lit.
    ContainerVisual unlit;
    ContainerVisual lit;
    CompositionColorBrush numericBrush;
    CompositionColorBrush actionBrush;
    CompositionColorBrush black;
    AmbientLight ambient;
    DistantLight distant;
    SpotLight spot;
    PointLight point;
    std::vector<DrawingSurface> maps;
    std::vector<DrawingSurface> shapes;
    std::vector<SpriteVisual> sprites;
    std::vector<CompositionRoundedRectangleGeometry> outlines;
    core::nullable<CompositionPropertySet> pointer;
    bool shining = false;

    Keys()
        : compositor(CompositionTarget::getCompositorForCurrentThread()),
          root(compositor.createContainerVisual()),
          unlit(compositor.createContainerVisual()),
          lit(compositor.createContainerVisual()),
          numericBrush(compositor.createColorBrush(numericColor.get())),
          actionBrush(compositor.createColorBrush(actionColor.get())),
          black(compositor.createColorBrush(rgb(0, 0, 0))),
          ambient(compositor.createAmbientLight()),
          distant(compositor.createDistantLight()),
          spot(compositor.createSpotLight()),
          point(compositor.createPointLight()) {
        root.size({lab::stageWidth, lab::stageHeight});
        unlit.relativeSizeAdjustment({1.0f, 1.0f});
        lit.relativeSizeAdjustment({1.0f, 1.0f});
        root.children().insertAtTop(unlit);
        root.children().insertAtTop(lit);

        for (Form const& form : forms) {
            SizeInt32 const size {static_cast<int>(form.width) * density, static_cast<int>(form.height) * density};
            maps.emplace_back(compositor, size);
            shapes.emplace_back(compositor, size);
        }
        makeKeys();

        distant.coordinateSpace(root);
        spot.coordinateSpace(root);
        spot.direction({0.0f, 0.0f, -1.0f});
        spot.innerConeAngle(0.0f);
        spot.innerConeIntensity(0.0f);
        spot.outerConeIntensity(0.0f);
        point.coordinateSpace(root);
        point.intensity(0.0f);
        ambient.targets().add(lit);
        distant.targets().add(lit);

        numericColor.on_change([this](Color const& value) noexcept { numericBrush.color(value); });
        actionColor.on_change([this](Color const& value) noexcept { actionBrush.color(value); });
        lab::onAny([this] { reshape(); }, emboss, rim, slope, rounded, corner);
        lab::onAny([this] { paint(); }, specularAmount, specularShine);
        lab::onAny([this] { relight(); }, shadow, azimuth, elevation, lampColor, ambientColor);
        lab::onAny([this] { aim(); }, lampKind, spotSize, spotCore, spotHeight, spotColor, linearAttenuation, quadraticAttenuation);
        lab::onAny([this] { fade(); }, spotIntensity, pointerOn);
        hardBorder.on_change([this](bool const&) noexcept { edge(); });

        reshape();
        paint();
        relight();
        aim();
    }

    // Сцена встаёт визуалом-ребёнком элемента; указатель над элементом ведёт лампу.
    void attach(Border const& host) {
        ElementCompositionPreview::setElementChildVisual(host, root);
        pointer = ElementCompositionPreview::getPointerPositionPropertySet(host);
        aim();
    }

    void makeKeys() {
        for (Place const& place : places) {
            Form const& form = forms[kinds[place.kind].form];

            // Обводка шире клавиши на единицу с каждой стороны, как рамка кнопки.
            auto const outline = compositor.createRoundedRectangleGeometry();
            outline.size({form.width + 2.0f, form.height + 2.0f});
            auto const border = compositor.createSpriteVisual();
            border.size({form.width + 2.0f, form.height + 2.0f});
            border.offset({place.left - 1.0f, place.top - 1.0f, 0.0f});
            border.brush(black);
            border.clip(compositor.createGeometricClip(outline));
            unlit.children().insertAtTop(border);
            outlines.push_back(outline);

            auto const sprite = compositor.createSpriteVisual();
            sprite.size({form.width, form.height});
            sprite.offset({place.left, place.top, 0.0f});
            lit.children().insertAtTop(sprite);
            sprites.push_back(sprite);
        }
        edge();
    }

    void edge() noexcept {
        lab::attempt(brushStatus, u"Граница спрайтов", [this] {
            for (auto const& sprite : sprites) {
                sprite.borderMode(hardBorder.get() ? CompositionBorderMode::Hard : CompositionBorderMode::Soft);
            }
        });
    }

    void reshape() noexcept {
        lab::attempt(mapStatus, u"Карты нормалей клавиш", [this] {
            float const radius = static_cast<float>(corner.get());
            for (int index = 0; index < formCount; ++index) {
                Form const& form = forms[index];
                lab::drawKeyMap(maps[index], lab::KeyRelief {
                                                 form.width,
                                                 form.height,
                                                 corner.get(),
                                                 rim.get(),
                                                 slope.get(),
                                                 rounded.get(),
                                                 emboss.get() * form.embossSign,
                                             });
                lab::drawShape(shapes[index], radius * density);
            }
            for (auto const& outline : outlines) {
                outline.cornerRadius({radius + 1.0f, radius + 1.0f});
            }
        });
    }

    // Граф клавиши: цвет, умноженный на общий и рассеянный свет, плюс блик, не
    // крашенный цветом; сверху вырезание по альфе формы.
    void paint() noexcept {
        lab::attempt(brushStatus, u"Кисть клавиши", [this] {
            auto const source = [](char16_t const* name) {
                return GraphicsEffectSource {CompositionEffectSourceParameter {name}};
            };

            SceneLightingEffect matte;
            matte.name(u"Matte");
            matte.normalMapSource(source(u"NormalMap"));
            matte.ambientAmount(1.0f);
            matte.diffuseAmount(1.0f);
            matte.specularAmount(0.0f);

            SceneLightingEffect gloss;
            gloss.name(u"Gloss");
            gloss.normalMapSource(source(u"NormalMap"));
            gloss.ambientAmount(0.0f);
            gloss.diffuseAmount(0.0f);
            gloss.specularAmount(static_cast<float>(specularAmount.get()));
            gloss.specularShine(static_cast<float>(specularShine.get()));

            ArithmeticCompositeEffect tinted;
            tinted.source1(source(u"Base"));
            tinted.source2(matte);
            tinted.multiplyAmount(1.0f);
            tinted.source1Amount(0.0f);
            tinted.source2Amount(0.0f);

            ArithmeticCompositeEffect whole;
            whole.source1(tinted);
            whole.source2(gloss);
            whole.multiplyAmount(0.0f);
            whole.source1Amount(1.0f);
            whole.source2Amount(1.0f);

            CompositeEffect cut;
            cut.mode(CanvasComposite::DestinationIn);
            cut.sources().append(whole);
            cut.sources().append(source(u"Shape"));

            auto const factory = compositor.createEffectFactory(cut);

            // По кисти на вид клавиши: клавиши одного вида делят и карту, и кисть.
            std::vector<CompositionEffectBrush> brushes;
            for (Kind const& kind : kinds) {
                auto const brush = factory.createBrush();
                auto const map = maps[kind.form].brush();
                map.stretch(CompositionStretch::Fill);
                brush.setSourceParameter(u"NormalMap", map);
                brush.setSourceParameter(u"Base", kind.action ? actionBrush : numericBrush);
                auto const shape = shapes[kind.form].brush();
                shape.stretch(CompositionStretch::Fill);
                brush.setSourceParameter(u"Shape", shape);
                brushes.push_back(brush);
            }
            for (std::size_t index = 0; index < places.size(); ++index) {
                sprites[index].brush(brushes[places[index].kind]);
            }
        });
    }

    // Плоское место клавиши остаётся её цветом при любом shadow: общий свет
    // даёт 1 - shadow, лампа на плоском -- shadow. Лампа стоит под азимутом и
    // высотой: 0 -- справа, 90 -- сверху экрана; свет идёт от неё.
    void relight() noexcept {
        lab::attempt(lightStatus, u"Постоянный свет", [this] {
            double const a = azimuth.get() * pi / 180.0;
            double const e = elevation.get() * pi / 180.0;
            double const share = shadow.get();

            ambient.color(ambientColor.get());
            ambient.intensity(static_cast<float>(1.0 - share));
            distant.color(lampColor.get());
            distant.intensity(static_cast<float>(share / std::sin(e)));
            distant.direction({static_cast<float>(-std::cos(a) * std::cos(e)),
                               static_cast<float>(std::sin(a) * std::cos(e)), static_cast<float>(-std::sin(e))});
        });
    }

    // Лампа стоит над указателем на заданной высоте. Прожектор смотрит отвесно
    // вниз, и конус даёт пятно заданного поперечника на плоскости клавиш; у
    // точечной лампы конуса нет. На клавиши нацелена одна из двух: визуалу
    // разрешено не больше двух источников помимо общего света.
    void aim() noexcept {
        lab::attempt(lightStatus, u"Лампа над указателем", [this] {
            bool const pointed = lampKind.get() == 1;
            float const z = static_cast<float>(spotHeight.get());
            float const linear = static_cast<float>(linearAttenuation.get());
            float const quadratic = static_cast<float>(quadraticAttenuation.get());

            spot.targets().removeAll();
            point.targets().removeAll();

            spot.innerConeColor(spotColor.get());
            spot.outerConeColor(spotColor.get());
            double const outer = std::atan2(spotSize.get() / 2.0, spotHeight.get());
            spot.innerConeAngle(static_cast<float>(std::min(std::atan2(spotCore.get() / 2.0, spotHeight.get()), outer)));
            spot.outerConeAngle(static_cast<float>(outer));
            spot.linearAttenuation(linear);
            spot.quadraticAttenuation(quadratic);
            point.color(spotColor.get());
            point.linearAttenuation(linear);
            point.quadraticAttenuation(quadratic);

            if (pointer) {
                auto const over = compositor.createExpressionAnimation(u"pointer.Position + Vector3(0, 0, z)");
                over.setReferenceParameter(u"pointer", *pointer);
                over.setScalarParameter(u"z", z);
                spot.startAnimation(u"Offset", over);
                point.startAnimation(u"Offset", over);
            }
            if (pointed) {
                point.targets().add(lit);
            } else {
                spot.targets().add(lit);
            }
        });
        fade();
    }

    // Лампа гасится яркостью, а не отключением: один ключевой кадр, в конце.
    void fade() noexcept {
        lab::attempt(lightStatus, u"Яркость лампы над указателем", [this] {
            bool const on = shining && pointerOn.get();
            auto const to = compositor.createScalarKeyFrameAnimation();
            to.insertKeyFrame(1.0f, on ? static_cast<float>(spotIntensity.get()) : 0.0f);
            to.duration(fadeTime);
            spot.startAnimation(u"InnerConeIntensity", to);
            spot.startAnimation(u"OuterConeIntensity", to);
            point.startAnimation(u"Intensity", to);
        });
    }

    void shine(bool on) noexcept {
        if (shining == on) {
            return;
        }
        shining = on;
        fade();
    }
};

}  // namespace

lab::Experiment lab::keyPage() {
    auto const model = std::make_shared<Keys>();

    auto stage = Grid {
        width = double {lab::stageWidth},
        height = double {lab::stageHeight},
        hAlign.center,
        vAlign.center,
        requestedTheme = ElementTheme::Dark,
        Border {
            background = SolidColorBrush {color = rgb(30, 31, 34)},
            CornerRadius {12},
            onPointerEntered = [model] { model->shine(true); },
            onPointerMoved = [model] { model->shine(true); },
            onPointerExited = [model] { model->shine(false); },
            [model](Border const& host) { model->attach(host); },
        },
    };

    // Знаки клавиш -- текст XAML поверх сцены; указателю он прозрачен.
    for (Place const& place : model->places) {
        Kind const& kind = kinds[place.kind];
        Form const& form = forms[kind.form];
        stage.children().append(Border {
            width = double {form.width},
            height = double {form.height},
            hAlign.left,
            vAlign.top,
            Margin {place.left, place.top, 0, 0},
            isHitTestVisible = false,
            TextBlock {
                place.label,
                hAlign.center,
                vAlign.center,
                fontSize = 22,
                foreground = SolidColorBrush {color = kind.action ? rgb(255, 227, 180) : rgb(237, 239, 242)},
            },
        });
    }

    auto settings = lab::settingsPanel({
        lab::noteRow(u"Форму клавиши задаёт карта нормалей: кант по краю и лицо чашей или куполом. Светят общий "
                     u"свет, постоянная далёкая лампа и прожектор над указателем. Чёрная обводка вне света. "
                     u"Нижняя большая клавиша -- с обратным emboss."),
        lab::statusRow(model->mapStatus),
        lab::statusRow(model->brushStatus),
        lab::statusRow(model->lightStatus),
        lab::group(u"Форма клавиши", true,
                   {
                       lab::sliderRow(u"emboss: чаша -- купол", model->emboss, -1.0, 1.0, 0.05),
                       lab::sliderRow(u"Ширина канта", model->rim, 0.0, 16.0, 0.5),
                       lab::sliderRow(u"Наклон канта (тангенс)", model->slope, 0.0, 4.0, 0.05),
                       lab::toggleRow(u"Кант скруглён", model->rounded),
                       lab::sliderRow(u"Скругление угла", model->corner, 0.0, 24.0, 0.5),
                   }),
        lab::group(u"Постоянный свет", true,
                   {
                       lab::noteRow(u"shadow делит свет: общий даёт 1 − shadow, лампа на плоском месте -- shadow. "
                                    u"Плоское место остаётся цветом клавиши."),
                       lab::sliderRow(u"shadow", model->shadow, 0.0, 1.0, 0.01),
                       lab::sliderRow(u"Азимут постоянной лампы", model->azimuth, 0.0, 360.0, 1.0),
                       lab::sliderRow(u"Высота постоянной лампы", model->elevation, 10.0, 90.0, 1.0),
                       lab::colorRow(u"Цвет постоянной лампы", model->lampColor),
                       lab::colorRow(u"Цвет общего света", model->ambientColor),
                   }),
        lab::group(u"Лампа над указателем", true,
                   {
                       lab::toggleRow(u"Лампа над указателем включена", model->pointerOn),
                       lab::choiceRow(u"Вид лампы над указателем", model->lampKind, lampKinds),
                       lab::sliderRow(u"Указатель: поперечник пятна (SpotLight)", model->spotSize, 10.0, 1200.0, 1.0),
                       lab::sliderRow(u"Указатель: поперечник полного света (SpotLight)", model->spotCore, 0.0, 1200.0, 1.0),
                       lab::sliderRow(u"Указатель: высота лампы", model->spotHeight, 5.0, 600.0, 1.0),
                       lab::sliderRow(u"Указатель: яркость", model->spotIntensity, 0.0, 4.0, 0.01),
                       lab::sliderRow(u"Указатель: LinearAttenuation", model->linearAttenuation, 0.0, 2.0, 0.001),
                       lab::sliderRow(u"Указатель: QuadraticAttenuation", model->quadraticAttenuation, 0.0, 1.0, 0.0001),
                       lab::colorRow(u"Цвет лампы над указателем", model->spotColor),
                   }),
        lab::group(u"Материал", false,
                   {
                       lab::sliderRow(u"Блик: SpecularAmount", model->specularAmount, 0.0, 2.0, 0.01),
                       lab::sliderRow(u"Блик: SpecularShine", model->specularShine, 1.0, 200.0, 1.0),
                       lab::colorRow(u"Цвет цифровых клавиш", model->numericColor),
                       lab::colorRow(u"Цвет клавиш действия", model->actionColor),
                       lab::toggleRow(u"Жёсткая граница спрайта (BorderMode Hard)", model->hardBorder),
                   }),
    });

    return {stage, settings, model};
}
