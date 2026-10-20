// Опыт «Матрица нормалей».
//
// Вопрос опыта: откуда композитор XAML этого потока берёт поворот нормалей
// карты и что с ним делает мягкая граница спрайта. Плитки -- спрайты с кистью
// из одного SceneLightingEffect, карта нормалей -- его вход; светят общий свет
// и одна далёкая лампа. Каждое преобразование стоит дважды: верхняя плитка с
// жёсткой границей, нижняя с мягкой.
//
// Карта на выбор. Конус -- нормаль наклонена от середины: светлая сторона
// круга показывает, откуда плитка видит лампу, какой бы поворот ни стоял.
// Постоянная нормаль -- вся плитка одного тона, если поворот нормалей один на
// всю плитку, и пятнами, если он свой у каждого куска сетки. Две постоянные
// карты под двумя азимутами лампы дают всю матрицу, через которую проходят
// нормали.
//
// Первую пару плиток вертят ползунки: поворот её кисти и поворот её спрайта.

#include "Lab.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr double pi = 3.14159265358979;

constexpr float tile = 72.0f;
constexpr int columns = 7;
constexpr float cellWidth = 100.0f, cellHeight = 115.0f;
constexpr float gridLeft = 10.0f, gridTop = 8.0f;

// Наклон нормали карты: тангенс 30 градусов.
constexpr double slope = 0.57735;

enum class What {
    plain,
    brushTurn30,
    brushTurn90,
    brushTurn180,
    brushScale,
    brushSqueeze,
    brushMirror,
    spriteTurn30,
    spriteSqueeze,
    parentTurn30,
    density1,
    densityUneven,
    roundedClip,
    circleClip,
};

struct Config {
    What what;
    char16_t const* name;
};

constexpr Config configs[] = {
    {What::plain, u"по ползункам"},
    {What::brushTurn30, u"кисть: поворот 30°"},
    {What::brushTurn90, u"кисть: поворот 90°"},
    {What::brushTurn180, u"кисть: поворот 180°"},
    {What::brushScale, u"кисть: масштаб 0,7"},
    {What::brushSqueeze, u"кисть: масштаб 1 × 0,6"},
    {What::brushMirror, u"кисть: зеркало по x"},
    {What::spriteTurn30, u"спрайт: поворот 30°"},
    {What::spriteSqueeze, u"спрайт: масштаб 1 × 0,6"},
    {What::parentTurn30, u"родитель: поворот 30°"},
    {What::density1, u"карта 72 × 72"},
    {What::densityUneven, u"карта 216 × 108"},
    {What::roundedClip, u"обрезка: скругление 8"},
    {What::circleClip, u"обрезка: круг"},
};
constexpr int configCount = static_cast<int>(std::size(configs));

constexpr char16_t const* mapNames[] = {
    u"Конус: нормаль от середины",
    u"Постоянная нормаль вправо",
    u"Постоянная нормаль вверх",
};

// Поверхности карты: обычная, вдвое плотнее плитки; плотности один; и с
// неравной плотностью по осям -- её растяжение на плитку неравномерно.
constexpr SizeInt32 surfaceSizes[] = {{144, 144}, {72, 72}, {216, 108}};

struct Tile {
    int config;
    bool hard;
    float left, top;
    SpriteVisual sprite;
};

struct Normals {
    core::observable<int> map {0};
    core::observable<double> azimuth {20.0};
    core::observable<double> elevation {45.0};
    // Повороты первой пары плиток: её кисть и её спрайт вертят ползунки, при
    // нулях это плитка как есть.
    core::observable<double> brushTurn {0.0};
    core::observable<double> spriteTurn {0.0};
    core::observable<hstring> status;

    Compositor compositor;
    ContainerVisual root;
    AmbientLight ambient;
    DistantLight distant;
    std::vector<DrawingSurface> surfaces;
    std::vector<Tile> tiles;

    Normals()
        : compositor(CompositionTarget::getCompositorForCurrentThread()),
          root(compositor.createContainerVisual()),
          ambient(compositor.createAmbientLight()),
          distant(compositor.createDistantLight()) {
        root.size({lab::stageWidth, lab::stageHeight});
        for (SizeInt32 const size : surfaceSizes) {
            surfaces.emplace_back(compositor, size);
        }
        makeTiles();

        distant.coordinateSpace(root);
        ambient.targets().add(root);
        distant.targets().add(root);

        map.on_change([this](int const&) noexcept { redraw(); });
        lab::onAny([this] { relight(); }, azimuth, elevation);
        brushTurn.on_change([this](double const&) noexcept { paint(); });
        spriteTurn.on_change([this](double const&) noexcept { turn(); });

        redraw();
        paint();
        relight();
    }

    void attach(Border const& host) { ElementCompositionPreview::setElementChildVisual(host, root); }

    // Два блока по семь преобразований; в блоке верхний ряд -- жёсткая граница,
    // нижний -- мягкая.
    void makeTiles() {
        for (int index = 0; index < configCount; ++index) {
            for (bool const hard : {true, false}) {
                int const row = (index / columns) * 2 + (hard ? 0 : 1);
                float const left = gridLeft + static_cast<float>(index % columns) * cellWidth + (cellWidth - tile) / 2;
                float const top = gridTop + static_cast<float>(row) * cellHeight;

                auto const sprite = compositor.createSpriteVisual();
                sprite.size({tile, tile});
                sprite.borderMode(hard ? CompositionBorderMode::Hard : CompositionBorderMode::Soft);

                Vector3 const middle {tile / 2, tile / 2, 0.0f};
                switch (configs[index].what) {
                case What::spriteTurn30:
                    sprite.centerPoint(middle);
                    sprite.rotationAngleInDegrees(30.0f);
                    break;
                case What::spriteSqueeze:
                    sprite.centerPoint(middle);
                    sprite.scale({1.0f, 0.6f, 1.0f});
                    break;
                case What::roundedClip:
                case What::circleClip: {
                    float const radius = configs[index].what == What::circleClip ? tile / 2 : 8.0f;
                    auto const outline = compositor.createRoundedRectangleGeometry();
                    outline.size({tile, tile});
                    outline.cornerRadius({radius, radius});
                    sprite.clip(compositor.createGeometricClip(outline));
                    break;
                }
                default:
                    break;
                }

                if (configs[index].what == What::parentTurn30) {
                    // Поворачивается контейнер, спрайт в нём стоит прямо.
                    auto const parent = compositor.createContainerVisual();
                    parent.size({tile, tile});
                    parent.offset({left, top, 0.0f});
                    parent.centerPoint(middle);
                    parent.rotationAngleInDegrees(30.0f);
                    parent.children().insertAtTop(sprite);
                    root.children().insertAtTop(parent);
                } else {
                    sprite.offset({left, top, 0.0f});
                    root.children().insertAtTop(sprite);
                }
                tiles.push_back({index, hard, left, top, sprite});
            }
        }
    }

    void turn() noexcept {
        lab::attempt(status, u"Поворот спрайта", [this] {
            for (Tile const& placed : tiles) {
                if (configs[placed.config].what == What::plain) {
                    placed.sprite.centerPoint({tile / 2, tile / 2, 0.0f});
                    placed.sprite.rotationAngleInDegrees(static_cast<float>(spriteTurn.get()));
                }
            }
        });
    }

    void redraw() noexcept {
        lab::attempt(status, mapNames[std::clamp(map.get(), 0, 2)], [this] {
            for (auto const& surface : surfaces) {
                switch (map.get()) {
                case 0:
                    lab::drawConeMap(surface, slope);
                    break;
                case 1:
                    lab::drawTiltMap(surface, slope, 0.0);
                    break;
                default:
                    lab::drawTiltMap(surface, 0.0, slope);
                    break;
                }
            }
        });
    }

    CompositionSurfaceBrush mapBrush(What what) const {
        int const kind = what == What::density1 ? 1 : what == What::densityUneven ? 2 : 0;
        auto const brush = surfaces[kind].brush();
        brush.stretch(CompositionStretch::Fill);

        Vector2 const middle {tile / 2, tile / 2};
        switch (what) {
        case What::plain:
            brush.centerPoint(middle);
            brush.rotationAngleInDegrees(static_cast<float>(brushTurn.get()));
            break;
        case What::brushTurn30:
            brush.centerPoint(middle);
            brush.rotationAngleInDegrees(30.0f);
            break;
        case What::brushTurn90:
            brush.centerPoint(middle);
            brush.rotationAngleInDegrees(90.0f);
            break;
        case What::brushTurn180:
            brush.centerPoint(middle);
            brush.rotationAngleInDegrees(180.0f);
            break;
        case What::brushScale:
            brush.centerPoint(middle);
            brush.scale({0.7f, 0.7f});
            break;
        case What::brushSqueeze:
            brush.centerPoint(middle);
            brush.scale({1.0f, 0.6f});
            break;
        case What::brushMirror:
            brush.centerPoint(middle);
            brush.scale({-1.0f, 1.0f});
            break;
        default:
            break;
        }
        return brush;
    }

    // Кисть плитки -- один SceneLightingEffect: белая поверхность под светом.
    void paint() noexcept {
        lab::attempt(status, u"Кисти плиток", [this] {
            SceneLightingEffect light;
            light.name(u"Light");
            light.normalMapSource(GraphicsEffectSource {CompositionEffectSourceParameter {u"NormalMap"}});
            light.ambientAmount(1.0f);
            light.diffuseAmount(1.0f);
            light.specularAmount(0.0f);
            auto const factory = compositor.createEffectFactory(light);

            for (Tile const& tile : tiles) {
                auto const brush = factory.createBrush();
                brush.setSourceParameter(u"NormalMap", mapBrush(configs[tile.config].what));
                tile.sprite.brush(brush);
            }
        });
    }

    // Лампа стоит под азимутом и высотой: 0 -- справа, 90 -- сверху экрана.
    void relight() noexcept {
        lab::attempt(status, u"Лампа", [this] {
            double const a = azimuth.get() * pi / 180.0;
            double const e = elevation.get() * pi / 180.0;
            ambient.color(rgb(255, 255, 255));
            ambient.intensity(0.25f);
            distant.color(rgb(255, 255, 255));
            distant.intensity(0.6f);
            distant.direction({static_cast<float>(-std::cos(a) * std::cos(e)),
                               static_cast<float>(std::sin(a) * std::cos(e)), static_cast<float>(-std::sin(e))});
        });
    }
};

}  // namespace

lab::Experiment lab::normalsPage() {
    auto const model = std::make_shared<Normals>();

    auto stage = Grid {
        width = double {lab::stageWidth},
        height = double {lab::stageHeight},
        hAlign.center,
        vAlign.center,
        requestedTheme = ElementTheme::Dark,
        Border {
            background = SolidColorBrush {color = rgb(30, 31, 34)},
            [model](Border const& host) { model->attach(host); },
        },
    };

    // Подписи под плитками: преобразование и граница.
    for (Tile const& placed : model->tiles) {
        stage.children().append(TextBlock {
            width = double {cellWidth - 4.0f},
            hAlign.left,
            vAlign.top,
            Margin {placed.left - (cellWidth - tile) / 2 + 2.0f, placed.top + tile + 2.0f, 0, 0},
            isHitTestVisible = false,
            fontSize = 10,
            textWrapping = TextWrapping::Wrap,
            textAlignment = TextAlignment::Center,
            opacity = 0.7,
            text = hstring {std::u16string {configs[placed.config].name} + (placed.hard ? u", Hard" : u", Soft")},
        });
    }

    auto settings = lab::settingsPanel({
        lab::noteRow(u"Каждое преобразование стоит дважды: верхняя плитка с жёсткой границей спрайта, нижняя с "
                     u"мягкой. У конуса светлая сторона круга показывает, откуда плитка видит лампу. Плитка с "
                     u"постоянной нормалью одного тона, если поворот нормалей один на всю плитку."),
        lab::noteRow(u"Замечено прогоном. С жёсткой границей нормали поворачиваются верно при любом преобразовании. С "
                     u"мягкой: у прямо стоящего спрайта ошибка есть, только когда кисть повёрнута ровно на 0, 90, "
                     u"180 или 270 градусов (на 36,9 и 53,1 градуса), а при любом другом повороте кисти свет верен; у "
                     u"повёрнутого спрайта ошибка есть почти при всех углах и меняется с углом, местами скачком; с "
                     u"обрезкой кругом свет верен, со скруглённым прямоугольником -- нет."),
        lab::statusRow(model->status),
        lab::choiceRow(u"Карта", model->map, mapNames),
        lab::sliderRow(u"Азимут лампы", model->azimuth, 0.0, 360.0, 1.0),
        lab::sliderRow(u"Высота лампы", model->elevation, 10.0, 90.0, 1.0),
        lab::sliderRow(u"Первая пара: поворот кисти", model->brushTurn, 0.0, 360.0, 1.0),
        lab::sliderRow(u"Первая пара: поворот спрайта", model->spriteTurn, 0.0, 360.0, 1.0),
    });

    return {stage, settings, model};
}
