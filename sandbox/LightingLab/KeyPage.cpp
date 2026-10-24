// Опыт «Клавиша на карте нормалей».
//
// Прототип света для Button3DEffect. Форма клавиши -- та же, что задают его
// параметры: тело, поднятое над панелью, с гранями канта по краю и лицом чашей
// или куполом (emboss). Здесь форма записана картой нормалей, а свет считает
// композитор XAML этого потока: общий свет, постоянная далёкая лампа (shadow
// делит свет между ними) и две лампы над указателем.
//
// Визуалу разрешено не больше двух источников помимо общего света, поэтому
// клавиша -- два спрайта с одной картой: грани и лицо поверх них. На лица
// нацелена лампа пятна, на грани -- своя, широкая: она высвечивает грани
// клавиш вокруг указателя, и ярче те, что к нему обращены. Чёрная обводка --
// отдельный визуал, на который лампы не нацелены.
//
// Нажатие -- движение тела, а не смена его формы. Клавиша стоит над панелью на
// высоте rise, панель -- визуал под клавишами на эту высоту ниже, и клавиша
// бросает на неё тень от постоянной лампы (CompositionProjectedShadow). Нажатая
// уходит вниз на travel: тень на панели короче, пока клавиша над панелью, а то,
// что ушло под панель, край отверстия заслоняет от лампы -- на верхней и левой
// грани ложится тень от внешнего края внутрь, и остаётся на них один общий
// свет. Форма граней и их наклон при этом те же, что у покоящейся клавиши. Тень
// края считается по той же формуле формы, что и карта нормалей, и ложится в
// кисть нажатой клавиши маской.
//
// Кроме движения тела, по флажкам: лампа пятна ныряет за пальцем, пятно
// вспыхивает, внутренняя часть клавиши уходит на единицу вниз и вправо, клавиша
// темнеет. Внутренняя часть -- знак, лицо и внутренний край канта: внешний
// край канта, силуэт клавиши и обводка стоят на месте, а сам кант слева и
// сверху становится на единицу шире, справа и снизу на единицу уже.
//
// Спрайты клавиш -- с жёсткой границей: при мягкой ответ карты нормалей на
// свет повёрнут. Гладкий край формы даёт маска в графе эффекта.

#include "Lab.h"

#include <wxl/Microsoft.UI.Input.h>
#include <wxl/Microsoft.UI.Xaml.Input.EventArgs.h>

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

// Клавиша «5»: её переключатель панели держит нажатой без указателя.
constexpr int heldKey = 5;

// На сколько нажатие сдвигает внутреннюю часть клавиши вниз и вправо.
constexpr float pressShift = 1.0f;

// Скругление панели -- то же, что у рамки сцены XAML под ней.
constexpr float panelCorner = 12.0f;

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

// Что кисть делает с картой нормалей перед тем, как отдать её графу.
enum class Turn { none, half, shifted, halfAside };

// На сколько сдвигает карту проба сдвига.
constexpr float probeShift = 12.0f;

// Лампа над указателем загорается и гаснет за fadeTime; нажатие действует
// быстрее, отпускание медленнее. Вспышка -- как у RevealEffect: втрое ярче
// сразу и обратно за flashTime.
constexpr std::chrono::milliseconds fadeTime {180};
constexpr std::chrono::milliseconds pressTime {120};
constexpr std::chrono::milliseconds releaseTime {250};
constexpr std::chrono::milliseconds flashTime {400};
constexpr float flashGain = 3.0f;

// Лампа над указателем: прожектор, который смотрит отвесно вниз. Внешний конус
// даёт пятно поперечником size на плоскости клавиш, внутренний -- круг
// поперечником full, где свет в полную силу: без него свет убывает от самой
// оси и до граней соседних клавиш доходит слабым. Высота, на которой лампа
// сейчас стоит, лежит в наборе свойств самой лампы: её анимирует нажатие.
struct Lamp {
    core::observable<bool> on {true};
    core::observable<double> size;
    core::observable<double> full;
    core::observable<double> height;
    core::observable<double> intensity;
    SpotLight light;

    Lamp(Compositor const& compositor, double size, double full, double height, double intensity)
        : size {size}, full {full}, height {height}, intensity {intensity}, light(compositor.createSpotLight()) {
        light.direction({0.0f, 0.0f, -1.0f});
        light.innerConeIntensity(0.0f);
        light.outerConeIntensity(0.0f);
        light.properties().insertScalar(u"Height", static_cast<float>(height));
    }
};

struct Keys {
    // Форма.
    core::observable<double> emboss {-1.0};
    core::observable<double> rim {3.0};
    core::observable<double> slope {1.0};
    core::observable<bool> rounded {false};
    core::observable<double> corner {8.0};
    // Ширина полосы вокруг стыка лица с кантом, где наклон канта переходит в
    // наклон лица, а свет одной лампы над указателем -- в свет другой.
    core::observable<double> smooth {2.0};

    // Тело над панелью: высота внешнего края канта над ней и ход нажатия.
    core::observable<double> rise {3.0};
    core::observable<double> travel {5.0};
    // Тень края отверстия: во сколько слабее полной и ширина полутени.
    core::observable<double> holeAmount {1.0};
    core::observable<double> holeSoft {1.0};
    // Падающая тень на панель: размытие в долях того, что даёт композитор.
    core::observable<double> castBlur {1.0};
    // Проба: панель без своего цвета -- ложится ли тень на прозрачный приёмник.
    core::observable<bool> clearPanel {false};

    // Постоянный свет: сколько его от лампы и сколько со всех сторон.
    core::observable<double> shadow {0.7};
    core::observable<double> azimuth {120.0};
    core::observable<double> elevation {50.0};
    core::observable<Color> lampColor {rgb(255, 255, 255)};
    core::observable<Color> ambientColor {rgb(255, 255, 255)};

    core::observable<Color> pointerColor {rgb(255, 255, 255)};

    // Проба преобразования кисти над клавиатурой: показать её или нет.
    core::observable<bool> probe {false};
    // Нажатая клавиша: что с ней происходит сверх движения тела. Отмеченное
    // действует вместе.
    core::observable<bool> pressDivesLamp {true};
    core::observable<bool> pressFlashes {false};
    core::observable<bool> pressMovesLabel {true};
    core::observable<bool> pressShades {false};
    core::observable<bool> hold {false};
    core::observable<double> diveDepth {0.33};
    core::observable<double> diveGain {2.0};
    core::observable<double> shadeAmount {0.2};
    // Клавиша, чей знак сейчас утоплен: знаки -- текст XAML и следят за полем.
    core::observable<int> labelDown {-1};

    // Материал.
    core::observable<double> specularAmount {0.15};
    core::observable<double> specularShine {30.0};
    core::observable<Color> numericColor {rgb(68, 72, 79)};
    core::observable<Color> actionColor {rgb(176, 98, 30)};
    core::observable<bool> hardBorder {true};

    core::observable<hstring> mapStatus;
    core::observable<hstring> brushStatus;
    core::observable<hstring> lightStatus;
    core::observable<hstring> shadowStatus;

    std::vector<Place> places = layout();

    Compositor compositor;
    ContainerVisual root;
    // Панель под клавишами, на rise ниже их: фон сцены, он же принимает
    // падающую тень. Скруглена как рамка сцены XAML, в которой стоит.
    SpriteVisual panel;
    // Снизу вверх: обводки вне света, грани, лица, притемнение нажатой клавиши.
    ContainerVisual unlit;
    ContainerVisual rims;
    ContainerVisual faces;
    ContainerVisual shades;
    ContainerVisual probes;
    // Освещённые клавиши пробы: на них светят только общий свет и постоянная лампа.
    ContainerVisual litProbes;
    std::vector<SpriteVisual> litProbeSprites;
    CompositionColorBrush numericBrush;
    CompositionColorBrush actionBrush;
    CompositionColorBrush black;
    // Падающая тень: чёрное с альфой в долю лампы -- на панели в тени остаётся
    // один общий свет.
    CompositionColorBrush shadowBrush;
    CompositionColorBrush panelBrush;
    AmbientLight ambient;
    DistantLight distant;
    // Пятно на лице клавиши под указателем и свет на гранях клавиш вокруг него.
    Lamp spot;
    Lamp edge;
    // Карты нормалей: клавиша и она же со сдвинутой внутренней частью.
    std::vector<DrawingSurface> maps;
    std::vector<DrawingSurface> pressedMaps;
    // Маски формы: вся клавиша и её лицо без канта.
    std::vector<DrawingSurface> wholeShapes;
    std::vector<DrawingSurface> faceShapes;
    // Тень края отверстия на гранях нажатой клавиши.
    std::vector<DrawingSurface> holeShadows;
    std::vector<SpriteVisual> rimSprites;
    std::vector<SpriteVisual> faceSprites;
    std::vector<SpriteVisual> shadeSprites;
    std::vector<CompositionRoundedRectangleGeometry> outlines;
    std::vector<CompositionRoundedRectangleGeometry> covers;
    // По кисти на вид клавиши: грани, грани нажатой клавиши, лицо.
    std::vector<CompositionEffectBrush> rimBrushes;
    std::vector<CompositionEffectBrush> pressedBrushes;
    std::vector<CompositionEffectBrush> faceBrushes;
    // Падающие тени, по одной на клавишу: кастеров у одной тени система
    // уважает лишь MaxRespectedCasters, у тени с одним кастером -- всегда его.
    std::vector<CompositionProjectedShadow> casts;
    core::nullable<CompositionPropertySet> pointer;
    bool shining = false;
    bool pointerDown = false;
    // Нажатая клавиша, номер в places, и нырнула ли с ней лампа пятна.
    int pressed = -1;
    bool dived = false;

    Keys()
        : compositor(CompositionTarget::getCompositorForCurrentThread()),
          root(compositor.createContainerVisual()),
          panel(compositor.createSpriteVisual()),
          unlit(compositor.createContainerVisual()),
          rims(compositor.createContainerVisual()),
          faces(compositor.createContainerVisual()),
          shades(compositor.createContainerVisual()),
          probes(compositor.createContainerVisual()),
          litProbes(compositor.createContainerVisual()),
          numericBrush(compositor.createColorBrush(numericColor.get())),
          actionBrush(compositor.createColorBrush(actionColor.get())),
          black(compositor.createColorBrush(rgb(0, 0, 0))),
          shadowBrush(compositor.createColorBrush(rgb(0, 0, 0))),
          panelBrush(compositor.createColorBrush(rgb(30, 31, 34))),
          ambient(compositor.createAmbientLight()),
          distant(compositor.createDistantLight()),
          spot(compositor, 260.0, 120.0, 30.0, 0.4),
          edge(compositor, 340.0, 200.0, 60.0, 1.5) {
        root.size({lab::stageWidth, lab::stageHeight});

        panel.size({lab::stageWidth, lab::stageHeight});
        panel.brush(panelBrush);
        auto const panelShape = compositor.createRoundedRectangleGeometry();
        panelShape.size({lab::stageWidth, lab::stageHeight});
        panelShape.cornerRadius({panelCorner, panelCorner});
        panel.clip(compositor.createGeometricClip(panelShape));
        root.children().insertAtTop(panel);

        for (ContainerVisual const* layer : {&unlit, &rims, &faces, &shades, &probes, &litProbes}) {
            layer->relativeSizeAdjustment({1.0f, 1.0f});
            root.children().insertAtTop(*layer);
        }

        for (Form const& form : forms) {
            SizeInt32 const size {static_cast<int>(form.width) * density, static_cast<int>(form.height) * density};
            maps.emplace_back(compositor, size);
            pressedMaps.emplace_back(compositor, size);
            wholeShapes.emplace_back(compositor, size);
            faceShapes.emplace_back(compositor, size);
            holeShadows.emplace_back(compositor, size);
        }
        makeKeys();
        makeProbe();
        makeCast();

        distant.coordinateSpace(root);
        for (ContainerVisual const* layer : {&rims, &faces, &litProbes}) {
            ambient.targets().add(*layer);
            distant.targets().add(*layer);
        }
        spot.light.targets().add(faces);
        edge.light.targets().add(rims);

        numericColor.on_change([this](Color const& value) noexcept { numericBrush.color(value); });
        actionColor.on_change([this](Color const& value) noexcept { actionBrush.color(value); });
        // Карта нажатой клавиши зависит от флажка сдвига: он её и перерисовывает.
        lab::onAny([this] { reshape(); }, emboss, rim, slope, rounded, corner, smooth, pressMovesLabel);
        // Тень края зависит и от формы -- её перерисовывает reshape() -- и от
        // хода с лампой.
        lab::onAny([this] { reshade(); }, rise, travel, holeAmount, holeSoft, shadow, azimuth, elevation);
        lab::onAny([this] { paint(); }, specularAmount, specularShine);
        lab::onAny([this] { relight(); }, shadow, azimuth, elevation, lampColor, ambientColor);
        rise.on_change([this](double const&) noexcept { sink(); });
        castBlur.on_change([this](double const&) noexcept { blur(); });
        clearPanel.on_change([this](bool const& clear) noexcept {
            panelBrush.color(clear ? rgba(30, 31, 34, 0.0) : rgb(30, 31, 34));
        });
        for (Lamp* lamp : {&spot, &edge}) {
            lamp->light.coordinateSpace(root);
            lab::onAny([this, lamp] { aim(*lamp); }, lamp->size, lamp->full, lamp->height, pointerColor);
            lab::onAny([this, lamp] { fade(*lamp, fadeTime); }, lamp->intensity, lamp->on);
            aim(*lamp);
        }
        hardBorder.on_change([this](bool const&) noexcept { sharpen(); });
        probe.on_change([this](bool const& on) noexcept {
            probes.isVisible(on);
            litProbes.isVisible(on);
        });
        hold.on_change([this](bool const& on) noexcept {
            if (!pointerDown) {
                setPressed(on ? heldKey : -1);
            }
        });

        reshape();
        paint();
        relight();
        sink();
    }

    // Сцена встаёт визуалом-ребёнком элемента; указатель над элементом ведёт лампы.
    void attach(Border const& host) {
        ElementCompositionPreview::setElementChildVisual(host, root);
        pointer = ElementCompositionPreview::getPointerPositionPropertySet(host);
        aim(spot);
        aim(edge);
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

            // Спрайт граней -- во всю клавишу: лицо лежит поверх и закрывает его
            // середину, так что на стыке лица и канта нет просвета до обводки.
            for (auto const& [layer, sprites] : {std::pair {&rims, &rimSprites}, std::pair {&faces, &faceSprites}}) {
                auto const sprite = compositor.createSpriteVisual();
                sprite.size({form.width, form.height});
                sprite.offset({place.left, place.top, 0.0f});
                layer->children().insertAtTop(sprite);
                sprites->push_back(sprite);
            }

            // Притемнение нажатой клавиши: чёрный спрайт поверх неё, вне света;
            // пока клавиша не нажата, он прозрачен.
            auto const cover = compositor.createRoundedRectangleGeometry();
            cover.size({form.width, form.height});
            auto const shade = compositor.createSpriteVisual();
            shade.size({form.width, form.height});
            shade.offset({place.left, place.top, 0.0f});
            shade.brush(black);
            shade.opacity(0.0f);
            shade.clip(compositor.createGeometricClip(cover));
            shades.children().insertAtTop(shade);
            shadeSprites.push_back(shade);
            covers.push_back(cover);
        }
        sharpen();
    }

    // Падающая тень: от постоянной лампы, бросают спрайты граней -- они во всю
    // клавишу, -- принимает панель. Тень только от далёкого источника: на
    // точечный и прожектор система отвечает «Unsupported light type».
    void makeCast() noexcept {
        lab::attempt(shadowStatus, u"CreateProjectedShadow", [this] {
            for (auto const& sprite : rimSprites) {
                auto const made = compositor.createProjectedShadow();
                made.lightSource(distant);
                auto const receiver = compositor.createProjectedShadowReceiver();
                receiver.receivingVisual(panel);
                made.receivers().add(receiver);
                auto const caster = compositor.createProjectedShadowCaster();
                caster.castingVisual(sprite);
                caster.brush(shadowBrush);
                made.casters().insertAtTop(caster);
                casts.push_back(made);
            }
        });
        blur();
        // Система считает тень не от всех кастеров одной тени, а от скольких --
        // говорит сама.
        std::u16string line {shadowStatus.get()};
        line += u"; теней ";
        core::append_number(line, static_cast<std::uint32_t>(casts.size()), 10);
        line += u" по кастеру, MaxRespectedCasters ";
        core::append_number(line, static_cast<std::uint32_t>(CompositionProjectedShadowCasterCollection::maxRespectedCasters()), 10);
        shadowStatus.set(hstring {line});
    }

    void blur() noexcept {
        lab::attempt(shadowStatus, u"CompositionProjectedShadow", [this] {
            for (auto const& made : casts) {
                made.blurRadiusMultiplier(static_cast<float>(castBlur.get()));
                made.minBlurRadius(0.0f);
                made.maxBlurRadius(8.0f);
            }
        });
    }

    // Кисть карты нормалей клавиши с преобразованием: пол-оборота вокруг
    // середины клавиши, сдвиг вправо или пол-оборота вокруг точки правее
    // середины на половину сдвига -- картинку он ставит туда же, куда сдвиг.
    CompositionSurfaceBrush mapBrush(DrawingSurface const& normals, Form const& form, Turn turn) const {
        auto const map = normals.brush();
        map.stretch(CompositionStretch::Fill);
        switch (turn) {
        case Turn::half:
            map.centerPoint({form.width / 2.0f, form.height / 2.0f});
            map.rotationAngleInDegrees(180.0f);
            break;
        case Turn::shifted:
            map.offset({probeShift, 0.0f});
            break;
        case Turn::halfAside:
            map.centerPoint({form.width / 2.0f + probeShift / 2.0f, form.height / 2.0f});
            map.rotationAngleInDegrees(180.0f);
            break;
        case Turn::none:
            break;
        }
        return map;
    }

    // Проба преобразования кисти на карте нормалей малой клавиши. Верхний ряд --
    // карта цветом: как есть; развёрнутая на пол-оборота и стоящая кистью
    // спрайта; развёрнутая так же, но поданная входом эффекта, который отдаёт
    // вход без изменений. Нижний ряд -- клавиши под постоянной лампой, карта --
    // вход NormalMapSource: как есть; развёрнутая; сдвинутая вправо; развёрнутая
    // вокруг точки правее середины. Свет у второй тот же, что у первой, а у
    // четвёртой тот же, что у третьей: вместе с картинкой композитор
    // поворачивает и сами нормали, так что разворот кисти кант не переворачивает.
    // Кисти нижнего ряда ставит paint().
    void makeProbe() {
        Form const& form = forms[0];

        ArithmeticCompositeEffect pass;
        pass.source1(GraphicsEffectSource {CompositionEffectSourceParameter {u"Map"}});
        pass.source2(GraphicsEffectSource {CompositionEffectSourceParameter {u"Map"}});
        pass.multiplyAmount(0.0f);
        pass.source1Amount(1.0f);
        pass.source2Amount(0.0f);
        auto const through = compositor.createEffectFactory(pass).createBrush();
        through.setSourceParameter(u"Map", mapBrush(maps[0], form, Turn::half));

        CompositionBrush const brushes[] = {mapBrush(maps[0], form, Turn::none),
                                            mapBrush(maps[0], form, Turn::half), through};
        for (int index = 0; index < 3; ++index) {
            auto const sprite = compositor.createSpriteVisual();
            sprite.size({form.width, form.height});
            sprite.offset({28.0f + static_cast<float>(index) * 96.0f, 18.0f, 0.0f});
            sprite.brush(brushes[index]);
            probes.children().insertAtTop(sprite);
        }
        for (int index = 0; index < 4; ++index) {
            auto const sprite = compositor.createSpriteVisual();
            sprite.size({form.width, form.height});
            sprite.offset({28.0f + static_cast<float>(index) * 96.0f, 408.0f, 0.0f});
            sprite.borderMode(CompositionBorderMode::Hard);
            litProbes.children().insertAtTop(sprite);
            litProbeSprites.push_back(sprite);
        }
        probes.isVisible(false);
        litProbes.isVisible(false);
    }

    void sharpen() noexcept {
        lab::attempt(brushStatus, u"Граница спрайтов", [this] {
            auto const mode = hardBorder.get() ? CompositionBorderMode::Hard : CompositionBorderMode::Soft;
            for (auto const& sprite : rimSprites) {
                sprite.borderMode(mode);
            }
            for (auto const& sprite : faceSprites) {
                sprite.borderMode(mode);
            }
        });
    }

    // Форма клавиши по настройкам; у нажатой внутренняя часть сдвинута, если
    // отмечено.
    lab::key_relief reliefOf(Form const& form, bool pressedKey) const {
        lab::key_relief key;
        key.width = form.width;
        key.height = form.height;
        key.corner = corner.get();
        key.rim = rim.get();
        key.slope = slope.get();
        key.rounded = rounded.get();
        key.emboss = emboss.get() * form.embossSign;
        key.smooth = smooth.get();
        if (pressedKey && pressMovesLabel.get()) {
            key.shiftX = pressShift;
            key.shiftY = pressShift;
        }
        return key;
    }

    void reshape() noexcept {
        lab::attempt(mapStatus, u"Карты нормалей клавиш", [this] {
            float const radius = static_cast<float>(corner.get());
            for (int index = 0; index < formCount; ++index) {
                lab::drawKeyMap(maps[index], reliefOf(forms[index], false));
                lab::drawFaceMask(faceShapes[index], reliefOf(forms[index], false));
                lab::drawShape(wholeShapes[index], radius * density);
                lab::drawKeyMap(pressedMaps[index], reliefOf(forms[index], true));
            }
            for (auto const& outline : outlines) {
                outline.cornerRadius({radius + 1.0f, radius + 1.0f});
            }
            for (auto const& cover : covers) {
                cover.cornerRadius({radius, radius});
            }
        });
        reshade();
    }

    // Тень края отверстия на нажатой клавише: её внешний край ниже панели на
    // travel - rise; пока ход меньше высоты, тени нет.
    void reshade() noexcept {
        lab::attempt(mapStatus, u"Тень края отверстия", [this] {
            lab::hole_shadow hole;
            hole.depth = travel.get() - rise.get();
            hole.azimuth = azimuth.get();
            hole.elevation = elevation.get();
            hole.share = shadow.get();
            hole.amount = holeAmount.get();
            hole.soft = holeSoft.get();
            for (int index = 0; index < formCount; ++index) {
                lab::drawHoleShadow(holeShadows[index], reliefOf(forms[index], true), hole);
            }
        });
    }

    // Граф клавиши: цвет, умноженный на общий и рассеянный свет, плюс блик, не
    // крашенный цветом; сверху вырезание по альфе формы. У граней и у лица граф
    // один, разные у них маска формы и лампа над указателем. У граней нажатой
    // клавиши перед вырезанием сверху ложится тень края отверстия.
    void paint() noexcept {
        lab::attempt(brushStatus, u"Кисть клавиши", [this] {
            auto const source = [](char16_t const* name) {
                return GraphicsEffectSource {CompositionEffectSourceParameter {name}};
            };

            auto const lit = [&] {
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

                // Сумма складывает и альфу: у обоих слагаемых она единица, выходит
                // два. Без обрезки полупрозрачный край маски лица вычитает спрайт
                // граней под собой, и на скруглениях стыка встают тёмные точки.
                ArithmeticCompositeEffect whole;
                whole.source1(tinted);
                whole.source2(gloss);
                whole.multiplyAmount(0.0f);
                whole.source1Amount(1.0f);
                whole.source2Amount(1.0f);
                whole.clampOutput(true);
                return whole;
            };

            auto const cutOf = [&](auto const& body) {
                CompositeEffect cut;
                cut.mode(CanvasComposite::DestinationIn);
                cut.sources().append(body);
                cut.sources().append(source(u"Shape"));
                return compositor.createEffectFactory(cut);
            };

            CompositeEffect shaded;
            shaded.mode(CanvasComposite::SourceOver);
            shaded.sources().append(lit());
            shaded.sources().append(source(u"HoleShadow"));

            auto const plain = cutOf(lit());
            auto const pressedFactory = cutOf(shaded);

            auto const maskOf = [](DrawingSurface const& surface) {
                auto const mask = surface.brush();
                mask.stretch(CompositionStretch::Fill);
                return mask;
            };
            auto const brushOf = [&](CompositionEffectFactory const& factory, Kind const& kind,
                                     DrawingSurface const& normals, DrawingSurface const& shape, Turn turn) {
                auto const brush = factory.createBrush();
                brush.setSourceParameter(u"NormalMap", mapBrush(normals, forms[kind.form], turn));
                brush.setSourceParameter(u"Base", kind.action ? actionBrush : numericBrush);
                brush.setSourceParameter(u"Shape", maskOf(shape));
                return brush;
            };
            rimBrushes.clear();
            pressedBrushes.clear();
            faceBrushes.clear();
            for (Kind const& kind : kinds) {
                rimBrushes.push_back(brushOf(plain, kind, maps[kind.form], wholeShapes[kind.form], Turn::none));
                // Нажатая клавиша -- вторая карта: развернуть первую кистью нельзя, вместе
                // с картинкой композитор поворачивает и нормали (проба ниже), а сдвиг
                // кисти оставил бы у края полосу без карты.
                auto const pressedBrush =
                    brushOf(pressedFactory, kind, pressedMaps[kind.form], wholeShapes[kind.form], Turn::none);
                pressedBrush.setSourceParameter(u"HoleShadow", maskOf(holeShadows[kind.form]));
                pressedBrushes.push_back(pressedBrush);
                faceBrushes.push_back(brushOf(plain, kind, maps[kind.form], faceShapes[kind.form], Turn::none));
            }
            constexpr Turn probeTurns[] = {Turn::none, Turn::half, Turn::shifted, Turn::halfAside};
            for (std::size_t index = 0; index < litProbeSprites.size(); ++index) {
                litProbeSprites[index].brush(brushOf(plain, kinds[0], maps[0], wholeShapes[0], probeTurns[index]));
            }
            for (int index = 0; index < static_cast<int>(places.size()); ++index) {
                rimSprites[index].brush((index == pressed ? pressedBrushes : rimBrushes)[places[index].kind]);
                faceSprites[index].brush(faceBrushes[places[index].kind]);
            }
        });
    }

    // Плоское место клавиши остаётся её цветом при любом shadow: общий свет
    // даёт 1 - shadow, лампа на плоском -- shadow. Лампа стоит под азимутом и
    // высотой: 0 -- справа, 90 -- сверху экрана; свет идёт от неё. Падающая
    // тень снимает с панели долю лампы.
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
            shadowBrush.color({static_cast<std::uint8_t>(std::lround(share * 255.0)), 0, 0, 0});
        });
    }

    // Панель на rise ниже клавиш: клавиши и лампы над ними стоят, где стояли.
    // Нажатая клавиша ушла вниз на ход, но не ниже панели: что ниже, тени на
    // панель не бросает.
    float depthOf(int index) const {
        return index == pressed ? -static_cast<float>(std::min(travel.get(), rise.get())) : 0.0f;
    }

    void sink() noexcept {
        panel.offset({0.0f, 0.0f, -static_cast<float>(rise.get())});
        for (int index = 0; index < static_cast<int>(places.size()); ++index) {
            Place const& place = places[index];
            rimSprites[index].offset({place.left, place.top, depthOf(index)});
        }
    }

    // Нырнуть может только лампа пятна: она идёт за пальцем.
    bool low(Lamp const& lamp) const { return dived && &lamp == &spot; }

    float heightOf(Lamp const& lamp) const {
        return static_cast<float>(lamp.height.get() * (low(lamp) ? diveDepth.get() : 1.0));
    }

    float intensityOf(Lamp const& lamp) const {
        if (!shining || !lamp.on.get()) {
            return 0.0f;
        }
        return static_cast<float>(lamp.intensity.get() * (low(lamp) ? diveGain.get() : 1.0));
    }

    // Лампа стоит над указателем на своей высоте: место считает композитор
    // выражением от набора свойств указателя и высоты из набора свойств лампы.
    // Конус задан по высоте покоя и с лампой не меняется: нырнувшая лампа даёт
    // пятно во столько же раз меньше.
    void aim(Lamp const& lamp) noexcept {
        lab::attempt(lightStatus, u"Лампа над указателем", [&] {
            double const outer = std::atan2(lamp.size.get() / 2.0, lamp.height.get());
            double const inner = std::min(std::atan2(lamp.full.get() / 2.0, lamp.height.get()), outer);
            lamp.light.innerConeColor(pointerColor.get());
            lamp.light.outerConeColor(pointerColor.get());
            lamp.light.innerConeAngle(static_cast<float>(inner));
            lamp.light.outerConeAngle(static_cast<float>(outer));
            lamp.light.properties().insertScalar(u"Height", heightOf(lamp));
            if (pointer) {
                auto const over =
                    compositor.createExpressionAnimation(u"pointer.Position + Vector3(0, 0, lamp.Height)");
                over.setReferenceParameter(u"pointer", *pointer);
                over.setReferenceParameter(u"lamp", lamp.light.properties());
                lamp.light.startAnimation(u"Offset", over);
            }
        });
    }

    // Яркость и высота идут к своему месту оттуда, где они сейчас: один
    // ключевой кадр, в конце. Лампа гасится яркостью, а не отключением.
    CompositionAnimation toward(float value, std::chrono::milliseconds time) const {
        auto const to = compositor.createScalarKeyFrameAnimation();
        to.insertKeyFrame(1.0f, value);
        to.duration(time);
        return to;
    }

    void fade(Lamp const& lamp, std::chrono::milliseconds time) noexcept {
        lab::attempt(lightStatus, u"Яркость лампы над указателем", [&] {
            auto const to = toward(intensityOf(lamp), time);
            lamp.light.startAnimation(u"InnerConeIntensity", to);
            lamp.light.startAnimation(u"OuterConeIntensity", to);
        });
    }

    void shine(bool on) noexcept {
        if (shining == on) {
            return;
        }
        shining = on;
        fade(spot, fadeTime);
        fade(edge, fadeTime);
    }

    // Клавиша под точкой сцены или -1.
    int keyAt(float x, float y) const {
        for (int index = 0; index < static_cast<int>(places.size()); ++index) {
            Place const& place = places[index];
            Form const& form = forms[kinds[place.kind].form];
            if (x >= place.left && x < place.left + form.width && y >= place.top && y < place.top + form.height) {
                return index;
            }
        }
        return -1;
    }

    void pointerPressed(float x, float y) noexcept {
        pointerDown = true;
        setPressed(keyAt(x, y));
    }

    void pointerReleased() noexcept {
        if (!pointerDown) {
            return;
        }
        pointerDown = false;
        setPressed(hold.get() ? heldKey : -1);
    }

    void setPressed(int index) noexcept {
        if (index == pressed) {
            return;
        }
        if (pressed >= 0) {
            release(pressed);
        }
        pressed = index;
        if (pressed >= 0) {
            press(pressed);
        }
    }

    // Со знаком уходит внутренняя часть клавиши, а внешний край канта стоит на
    // месте. Внутренний край канта сдвинут в самой карте нажатой клавиши; здесь
    // сдвигается спрайт лица, чья маска идёт по этому краю.
    void shift(int index, float by) {
        Place const& place = places[index];
        faceSprites[index].offset({place.left + by, place.top + by, 0.0f});
    }

    // Нажатие берёт то, что отмечено в эту минуту; отпускание возвращает всё,
    // что нажатие могло тронуть, что бы ни было отмечено теперь. Тело уходит
    // вниз за время нажатия и возвращается за время отпускания; тень края
    // отверстия приходит с кистью сразу.
    void press(int index) noexcept {
        lab::attempt(brushStatus, u"Нажатая клавиша", [&] {
            rimSprites[index].brush(pressedBrushes[places[index].kind]);
            rimSprites[index].startAnimation(u"Offset.Z", toward(depthOf(index), pressTime));
            if (pressShades.get()) {
                shadeSprites[index].startAnimation(u"Opacity",
                                                   toward(static_cast<float>(shadeAmount.get()), pressTime));
            }
            if (pressMovesLabel.get()) {
                labelDown.set(index);
                shift(index, pressShift);
            }
            if (pressDivesLamp.get()) {
                dived = true;
                spot.light.properties().startAnimation(u"Height", toward(heightOf(spot), pressTime));
            }
            if (pressFlashes.get() && intensityOf(spot) > 0.0f) {
                auto const pulse = compositor.createScalarKeyFrameAnimation();
                pulse.insertKeyFrame(0.0f, intensityOf(spot) * flashGain);
                pulse.insertKeyFrame(1.0f, intensityOf(spot));
                pulse.duration(flashTime);
                spot.light.startAnimation(u"InnerConeIntensity", pulse);
                spot.light.startAnimation(u"OuterConeIntensity", pulse);
            } else {
                fade(spot, pressTime);
            }
        });
    }

    void release(int index) noexcept {
        lab::attempt(brushStatus, u"Отпущенная клавиша", [&] {
            rimSprites[index].brush(rimBrushes[places[index].kind]);
            rimSprites[index].startAnimation(u"Offset.Z", toward(0.0f, releaseTime));
            shadeSprites[index].startAnimation(u"Opacity", toward(0.0f, releaseTime));
            labelDown.set(-1);
            shift(index, 0.0f);
            dived = false;
            spot.light.properties().startAnimation(u"Height", toward(heightOf(spot), releaseTime));
            fade(spot, releaseTime);
        });
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
            // Фон рисует панель сцены; прозрачный фон рамки нужен указателю:
            // без фона элемент не получает событий указателя.
            background = SolidColorBrush {color = rgba(30, 31, 34, 0.0)},
            CornerRadius {12},
            // Лампы зажигает движение, а не вход: сцена, вставшая под неподвижный
            // указатель, получает вход раньше, чем набор свойств узнаёт его место,
            // и лампы загорались бы в углу сцены.
            onPointerMoved += [model] { model->shine(true); },
            onPointerExited +=
                [model] {
                    model->pointerReleased();
                    model->shine(false);
                },
            onPointerPressed +=
                [model](Object const& sender, PointerRoutedEventArgs& args) {
                    auto const at = args.getCurrentPoint(sender.try_as<Border>()).position();
                    model->pointerPressed(static_cast<float>(at.x), static_cast<float>(at.y));
                },
            onPointerReleased += [model] { model->pointerReleased(); },
            onPointerCaptureLost += [model] { model->pointerReleased(); },
            [model](Border const& host) { model->attach(host); },
        },
    };

    // Знаки клавиш -- текст XAML поверх сцены; указателю он прозрачен. Знак
    // нажатой клавиши уходит на единицу вниз и вправо и тускнеет.
    for (int index = 0; index < static_cast<int>(model->places.size()); ++index) {
        Place const& place = model->places[index];
        Kind const& kind = kinds[place.kind];
        Form const& form = forms[kind.form];
        double const left = place.left, top = place.top;
        stage.children().append(Border {
            width = double {form.width},
            height = double {form.height},
            hAlign.left,
            vAlign.top,
            margin = BindOutput {model->labelDown,
                                 [index, left, top](int down) {
                                     double const shift = down == index ? 1.0 : 0.0;
                                     return Thickness {left + shift, top + shift, 0.0, 0.0};
                                 }},
            opacity = BindOutput {model->labelDown, [index](int down) { return down == index ? 0.55 : 1.0; }},
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
        lab::noteRow(u"Форму клавиши задаёт карта нормалей: грани канта по краю и лицо чашей или куполом. Светят "
                     u"общий свет, постоянная далёкая лампа и две лампы над указателем: одна кладёт пятно на лица "
                     u"клавиш, другая светит на их грани. Чёрная обводка вне света. Нижняя большая клавиша -- с "
                     u"обратным emboss."),
        lab::statusRow(model->mapStatus),
        lab::statusRow(model->brushStatus),
        lab::statusRow(model->lightStatus),
        lab::statusRow(model->shadowStatus),
        lab::group(u"Нажатая клавиша", true,
                   {
                       lab::noteRow(u"Нажмите клавишу мышью. Клавиша -- тело над панелью: нажатие уводит его вниз на "
                                    u"ход, падающая тень на панель короче, а грани, ушедшие под панель, сверху и "
                                    u"слева накрывает тень края отверстия -- от внешнего края внутрь, на них остаётся "
                                    u"один общий свет. Отмеченное ниже действует вместе с движением."),
                       lab::sliderRow(u"Высота клавиши над панелью", model->rise, 0.0, 8.0, 0.5),
                       lab::sliderRow(u"Ход нажатия", model->travel, 0.0, 8.0, 0.5),
                       lab::sliderRow(u"Тень края отверстия: доля полной", model->holeAmount, 0.0, 1.0, 0.05),
                       lab::sliderRow(u"Тень края отверстия: полутень, ширина", model->holeSoft, 0.0, 3.0, 0.25),
                       lab::sliderRow(u"Падающая тень: размытие, раз", model->castBlur, 0.0, 4.0, 0.1),
                       lab::toggleRow(u"Проба: панель прозрачна (тень на прозрачном приёмнике)", model->clearPanel),
                       lab::checkRow(u"Лампа пятна ныряет за пальцем", model->pressDivesLamp),
                       lab::checkRow(u"Вспышка пятна", model->pressFlashes),
                       lab::checkRow(u"Знак, лицо и внутренний край канта уходят вниз и вправо на единицу",
                                     model->pressMovesLabel),
                       lab::checkRow(u"Клавиша темнеет", model->pressShades),
                       lab::toggleRow(u"Держать клавишу 5 нажатой", model->hold),
                       lab::sliderRow(u"Нырок: доля высоты лампы", model->diveDepth, 0.1, 1.0, 0.01),
                       lab::sliderRow(u"Нырок: яркость, раз", model->diveGain, 1.0, 4.0, 0.05),
                       lab::sliderRow(u"Притемнение: доля", model->shadeAmount, 0.0, 0.6, 0.01),
                   }),
        lab::group(u"Проба: преобразование кисти и карта нормалей", false,
                   {
                       lab::noteRow(u"Верхний ряд -- карта нормалей цветом: как есть; развёрнутая кистью на пол-оборота; "
                                    u"развёрнутая так же и поданная входом эффекта. Картинка поворачивается. Нижний ряд -- "
                                    u"клавиши под постоянной лампой: карта как есть; развёрнутая; сдвинутая вправо; "
                                    u"развёрнутая вокруг точки правее середины. Свет второй равен свету первой, свет "
                                    u"четвёртой -- свету третьей: вместе с картинкой поворачиваются и нормали, поэтому "
                                    u"разворот кисти кант не переворачивает."),
                       lab::toggleRow(u"Показать пробу разворота кисти", model->probe),
                   }),
        lab::group(u"Пятно на лице: лампа над указателем", true,
                   {
                       lab::toggleRow(u"Пятно на лице включено", model->spot.on),
                       lab::sliderRow(u"Пятно: поперечник", model->spot.size, 10.0, 1200.0, 1.0),
                       lab::sliderRow(u"Пятно: поперечник полного света", model->spot.full, 0.0, 1200.0, 1.0),
                       lab::sliderRow(u"Пятно: высота лампы", model->spot.height, 5.0, 600.0, 1.0),
                       lab::sliderRow(u"Пятно: яркость", model->spot.intensity, 0.0, 4.0, 0.01),
                   }),
        lab::group(u"Свет на гранях: лампа над указателем", true,
                   {
                       lab::toggleRow(u"Свет на гранях включён", model->edge.on),
                       lab::sliderRow(u"Грани: поперечник", model->edge.size, 10.0, 1200.0, 1.0),
                       lab::sliderRow(u"Грани: поперечник полного света", model->edge.full, 0.0, 1200.0, 1.0),
                       lab::sliderRow(u"Грани: высота лампы", model->edge.height, 5.0, 600.0, 1.0),
                       lab::sliderRow(u"Грани: яркость", model->edge.intensity, 0.0, 6.0, 0.01),
                       lab::colorRow(u"Цвет ламп над указателем", model->pointerColor),
                   }),
        lab::group(u"Форма клавиши", true,
                   {
                       lab::sliderRow(u"emboss: чаша -- купол", model->emboss, -1.0, 1.0, 0.05),
                       lab::sliderRow(u"Ширина канта", model->rim, 0.0, 16.0, 0.5),
                       lab::sliderRow(u"Наклон канта (тангенс)", model->slope, 0.0, 4.0, 0.05),
                       lab::toggleRow(u"Кант скруглён", model->rounded),
                       lab::sliderRow(u"Скругление угла", model->corner, 0.0, 24.0, 0.5),
                       lab::sliderRow(u"Сглаживание стыка лица с кантом, ширина", model->smooth, 0.0, 6.0, 0.25),
                   }),
        lab::group(u"Постоянный свет", false,
                   {
                       lab::noteRow(u"shadow делит свет: общий даёт 1 − shadow, лампа на плоском месте -- shadow. "
                                    u"Плоское место остаётся цветом клавиши."),
                       lab::sliderRow(u"shadow", model->shadow, 0.0, 1.0, 0.01),
                       lab::sliderRow(u"Азимут постоянной лампы", model->azimuth, 0.0, 360.0, 1.0),
                       lab::sliderRow(u"Высота постоянной лампы", model->elevation, 10.0, 90.0, 1.0),
                       lab::colorRow(u"Цвет постоянной лампы", model->lampColor),
                       lab::colorRow(u"Цвет общего света", model->ambientColor),
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
