// Опыт «Scenes: клавиша настоящей геометрией».
//
// Microsoft.UI.Composition.Scenes рисует в дереве композитора трёхмерную
// сцену: узлы, сетки треугольников и материал по модели «металличность --
// шероховатость». Здесь клавиша -- не картинка с нормалями, а сетка: вершины
// подняты по профилю плеча и кривизне лица, нормали посчитаны по той же
// формуле. Шесть клавиш -- шесть узлов с общей сеткой и общим материалом.
//
// Обёрток wxl у этих типов нет, и вершины отдаются сетке буфером, до байтов
// которого добираются по COM-интерфейсу вне метаданных, -- поэтому этот файл
// смотрит в проекцию cppwinrt сам и отдаёт стенду готовый визуал.

#include "platform.h"

#include <MemoryBuffer.h>
#include <cstring>

#include <winrt/Microsoft.Graphics.DirectX.h>
#include <winrt/Microsoft.UI.Composition.Scenes.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Foundation.h>

#include "Lab.h"
#include "Object.impl.h"
#include <wxl/Microsoft.UI.Composition.impl.h>

using namespace wxl;
using namespace wxl::dsl;

namespace {

namespace composition = winrt::Microsoft::UI::Composition;
namespace scenes = winrt::Microsoft::UI::Composition::Scenes;
namespace directx = winrt::Microsoft::Graphics::DirectX;
using winrt::Windows::Foundation::Numerics::float3;
using winrt::Windows::Foundation::Numerics::float4;

constexpr char16_t const* lightKinds[] = {u"Без источников", u"PointLight за указателем", u"DistantLight"};

// Сетка клавиши: отрезков на сторону и на дугу угла, колец от края к середине.
constexpr int sideSteps = 12;
constexpr int arcSteps = 24;
constexpr int rings = 28;

// Буфер WinRT с байтами данных: до них добираются через IMemoryBufferByteAccess.
template <class T>
winrt::Windows::Foundation::MemoryBuffer bufferOf(std::vector<T> const& data) {
    auto const bytes = static_cast<uint32_t>(data.size() * sizeof(T));
    winrt::Windows::Foundation::MemoryBuffer buffer {bytes};
    auto const reference = buffer.CreateReference();
    auto const access = reference.as<::Windows::Foundation::IMemoryBufferByteAccess>();
    BYTE* target = nullptr;
    UINT32 capacity = 0;
    winrt::check_hresult(access->GetBuffer(&target, &capacity));
    std::memcpy(target, data.data(), std::min<std::size_t>(bytes, capacity));
    return buffer;
}

struct Solid {
    core::observable<double> metallic {0.1};
    core::observable<double> roughness {0.45};
    core::observable<Color> keyColor {rgb(68, 72, 79)};
    core::observable<double> rise {18.0};
    core::observable<double> shoulder {26.0};
    core::observable<double> corner {22.0};
    core::observable<double> emboss {-0.4};
    core::observable<double> tiltX {0.0};
    core::observable<double> tiltY {0.0};

    core::observable<int> lightKind {0};
    core::observable<double> lightHeight {220.0};
    core::observable<double> azimuth {120.0};
    core::observable<double> elevation {45.0};
    core::observable<bool> ambientOn {true};

    core::observable<hstring> sceneStatus;
    core::observable<hstring> lightStatus;

    Compositor compositor;
    AmbientLight ambient;
    PointLight point;
    DistantLight distant;
    core::nullable<Visual> stage;
    core::nullable<CompositionPropertySet> pointer;

    scenes::SceneVisual scene {nullptr};
    scenes::SceneNode pad {nullptr};
    scenes::SceneMesh mesh {nullptr};
    scenes::SceneMetallicRoughnessMaterial material {nullptr};

    Solid()
        : compositor(CompositionTarget::getCompositorForCurrentThread()),
          ambient(compositor.createAmbientLight()),
          point(compositor.createPointLight()),
          distant(compositor.createDistantLight()) {
        lab::attempt(sceneStatus, u"SceneVisual, SceneMesh, SceneMetallicRoughnessMaterial", [this] { build(); });

        lab::onAny([this] { shape(); }, rise, shoulder, corner, emboss);
        lab::onAny([this] { finish(); }, metallic, roughness, keyColor);
        lab::onAny([this] { turn(); }, tiltX, tiltY);
        lab::onAny([this] { relight(); }, lightKind, lightHeight, azimuth, elevation, ambientOn);
    }

    void build() {
        composition::Compositor const& native = *Object::Impl::get_typed<Compositor>(compositor);

        scene = scenes::SceneVisual::Create(native);
        scene.Size({lab::stageWidth, lab::stageHeight});
        // Начало сцены -- середина панели: клавиши стоят вокруг него.
        scene.Offset({lab::stageWidth / 2.0f, lab::stageHeight / 2.0f, 0.0f});

        pad = scenes::SceneNode::Create(native);
        scene.Root(pad);

        mesh = scenes::SceneMesh::Create(native);
        mesh.PrimitiveTopology(directx::DirectXPrimitiveTopology::TriangleList);

        material = scenes::SceneMetallicRoughnessMaterial::Create(native);
        // Ось y сцены может смотреть вверх, и тогда обход треугольников обратный.
        material.IsDoubleSided(true);

        for (int index = 0; index < lab::keyColumns * lab::keyRows; ++index) {
            auto const node = scenes::SceneNode::Create(native);
            float const x = lab::keyLeft(index % lab::keyColumns) + lab::keySide / 2.0f - lab::stageWidth / 2.0f;
            float const y = lab::keyTop(index / lab::keyColumns) + lab::keySide / 2.0f - lab::stageHeight / 2.0f;
            node.Transform().Translation({x, y, 0.0f});

            auto const renderer = scenes::SceneMeshRendererComponent::Create(native);
            renderer.Mesh(mesh);
            renderer.Material(material);
            node.Components().Append(renderer);
            pad.Children().Append(node);
        }

        stage = Object::Impl::wrap<Visual>(composition::Visual {scene});
        point.coordinateSpace(*stage);
        distant.coordinateSpace(*stage);

        fill();
        paint();
    }

    // Насколько точка, отсчитанная от середины клавиши, внутри её скруглённого края.
    double insideAt(double x, double y) const {
        double const half = lab::keySide / 2.0;
        double const radius = std::clamp(corner.get(), 0.0, half);
        double const qx = std::abs(x) - (half - radius), qy = std::abs(y) - (half - radius);
        double const ox = std::max(qx, 0.0), oy = std::max(qy, 0.0);
        return radius - std::sqrt(ox * ox + oy * oy) - std::min(std::max(qx, qy), 0.0);
    }

    // Высота клавиши над панелью в той же точке.
    double heightAt(double x, double y) const {
        double const half = lab::keySide / 2.0;
        double const inside = insideAt(x, y);
        if (inside <= 0.0) {
            return 0.0;
        }
        double const t = std::clamp(inside / std::max(shoulder.get(), 0.001), 0.0, 1.0);
        double const edge = std::sqrt(1.0 - (1.0 - t) * (1.0 - t));
        double const ux = x / half, uy = y / half;
        double const bowl = 1.0 - std::clamp((ux * ux + uy * uy) * 0.5, 0.0, 1.0);
        return rise.get() * edge * (1.0 + emboss.get() * (bowl - 1.0));
    }

    void fill() {
        // Край клавиши точками: четыре стороны и четыре дуги углов, по кругу. Каждый
        // отрезок отдаёт своё начало, а конец -- начало следующего.
        double const half = lab::keySide / 2.0;
        double const radius = std::clamp(corner.get(), 0.0, half);
        double const flat = half - radius;
        constexpr double quarter = 3.14159265358979 / 2.0;

        std::vector<std::pair<double, double>> outline;
        outline.reserve(4 * (sideSteps + arcSteps));
        auto const line = [&](double fromX, double fromY, double toX, double toY) {
            for (int step = 0; step < sideSteps; ++step) {
                double const t = static_cast<double>(step) / sideSteps;
                outline.emplace_back(fromX + (toX - fromX) * t, fromY + (toY - fromY) * t);
            }
        };
        auto const arc = [&](double centreX, double centreY, double from) {
            for (int step = 0; step < arcSteps; ++step) {
                double const angle = from + quarter * step / arcSteps;
                outline.emplace_back(centreX + radius * std::cos(angle), centreY + radius * std::sin(angle));
            }
        };
        line(-flat, -half, flat, -half);
        arc(flat, -flat, -quarter);
        line(half, -flat, half, flat);
        arc(flat, flat, 0.0);
        line(flat, half, -flat, half);
        arc(-flat, flat, quarter);
        line(-half, flat, -half, -flat);
        arc(-flat, -flat, 2.0 * quarter);

        // Сетка -- кольца, подобные краю и стянутые к середине: внешнее лежит на
        // самом скруглённом крае, поэтому силуэт гладкий. У края кольца чаще --
        // там плечо, и высота меняется быстро.
        int const around = static_cast<int>(outline.size());
        std::vector<float> positions;
        std::vector<float> normals;
        std::vector<std::uint16_t> indices;
        positions.reserve((around * rings + 1) * 3);
        normals.reserve((around * rings + 1) * 3);

        auto const vertex = [&](double x, double y) {
            // Нормаль -- по разности высот вокруг вершины.
            double const dx = heightAt(x + 0.5, y) - heightAt(x - 0.5, y);
            double const dy = heightAt(x, y + 0.5) - heightAt(x, y - 0.5);
            double const length = std::sqrt(dx * dx + dy * dy + 1.0);
            positions.insert(positions.end(),
                             {static_cast<float>(x), static_cast<float>(y), static_cast<float>(heightAt(x, y))});
            normals.insert(normals.end(), {static_cast<float>(-dx / length), static_cast<float>(-dy / length),
                                           static_cast<float>(1.0 / length)});
        };

        for (int ring = 0; ring < rings; ++ring) {
            double const scale = 1.0 - std::pow(static_cast<double>(ring) / rings, 1.6);
            for (auto const& [x, y] : outline) {
                vertex(x * scale, y * scale);
            }
        }
        vertex(0.0, 0.0);

        auto const at = [around](int ring, int place) {
            return static_cast<std::uint16_t>(ring * around + place % around);
        };
        auto const middle = static_cast<std::uint16_t>(around * rings);
        for (int ring = 0; ring < rings; ++ring) {
            for (int place = 0; place < around; ++place) {
                if (ring + 1 < rings) {
                    indices.insert(indices.end(), {at(ring, place), at(ring, place + 1), at(ring + 1, place),
                                                   at(ring, place + 1), at(ring + 1, place + 1), at(ring + 1, place)});
                } else {
                    indices.insert(indices.end(), {at(ring, place), at(ring, place + 1), middle});
                }
            }
        }

        mesh.FillMeshAttribute(scenes::SceneAttributeSemantic::Vertex, directx::DirectXPixelFormat::R32G32B32Float,
                               bufferOf(positions));
        mesh.FillMeshAttribute(scenes::SceneAttributeSemantic::Normal, directx::DirectXPixelFormat::R32G32B32Float,
                               bufferOf(normals));
        mesh.FillMeshAttribute(scenes::SceneAttributeSemantic::Index, directx::DirectXPixelFormat::R16UInt,
                               bufferOf(indices));
    }

    void paint() {
        Color const color = keyColor.get();
        material.BaseColorFactor(float4 {color.R / 255.0f, color.G / 255.0f, color.B / 255.0f, 1.0f});
        material.MetallicFactor(static_cast<float>(metallic.get()));
        material.RoughnessFactor(static_cast<float>(roughness.get()));
    }

    void shape() noexcept {
        if (mesh) {
            lab::attempt(sceneStatus, u"SceneMesh.FillMeshAttribute", [this] { fill(); });
        }
    }

    void finish() noexcept {
        if (material) {
            lab::attempt(sceneStatus, u"SceneMetallicRoughnessMaterial", [this] { paint(); });
        }
    }

    // Наклон всей клавиатуры: сцена трёхмерная, и это видно только в повороте.
    void turn() noexcept {
        if (!pad) {
            return;
        }
        lab::attempt(sceneStatus, u"SceneModelTransform", [this] {
            float const aroundX = static_cast<float>(tiltX.get() * 3.14159265358979 / 180.0);
            float const aroundY = static_cast<float>(tiltY.get() * 3.14159265358979 / 180.0);
            pad.Transform().Orientation(winrt::Windows::Foundation::Numerics::make_quaternion_from_yaw_pitch_roll(
                aroundY, aroundX, 0.0f));
        });
    }

    void attach(Border const& host) {
        if (!stage) {
            return;
        }
        ElementCompositionPreview::setElementChildVisual(host, *stage);
        pointer = ElementCompositionPreview::getPointerPositionPropertySet(host);
        relight();
    }

    void relight() noexcept {
        if (!stage) {
            return;
        }
        lab::attempt(lightStatus, u"Источник света на SceneVisual", [this] {
            ambient.targets().removeAll();
            point.targets().removeAll();
            distant.targets().removeAll();
            if (lightKind.get() == 0) {
                return;
            }
            if (ambientOn.get()) {
                ambient.targets().add(*stage);
            }

            if (lightKind.get() == 1) {
                point.stopAnimation(u"Offset");
                float const z = static_cast<float>(lightHeight.get());
                if (pointer) {
                    // Начало сцены сдвинуто в середину панели, указатель считается от её угла.
                    auto const follow = compositor.createExpressionAnimation(
                        u"pointer.Position + Vector3(-halfWidth, -halfHeight, z)");
                    follow.setReferenceParameter(u"pointer", *pointer);
                    follow.setScalarParameter(u"halfWidth", lab::stageWidth / 2.0f);
                    follow.setScalarParameter(u"halfHeight", lab::stageHeight / 2.0f);
                    follow.setScalarParameter(u"z", z);
                    point.startAnimation(u"Offset", follow);
                } else {
                    point.offset({0.0f, 0.0f, z});
                }
                point.targets().add(*stage);
            } else {
                double const a = azimuth.get() * 3.14159265358979 / 180.0;
                double const e = elevation.get() * 3.14159265358979 / 180.0;
                distant.direction({static_cast<float>(-std::cos(a) * std::cos(e)),
                                   static_cast<float>(std::sin(a) * std::cos(e)), static_cast<float>(-std::sin(e))});
                distant.targets().add(*stage);
            }
        });
    }
};

}  // namespace

lab::Experiment lab::scenesPage() {
    auto const model = std::make_shared<Solid>();

    auto stage = Border {
        width = double {lab::stageWidth},
        height = double {lab::stageHeight},
        hAlign.center,
        vAlign.center,
        background = SolidColorBrush {color = rgb(42, 44, 49)},
        [model](Border const& host) { model->attach(host); },
    };

    auto settings = lab::settingsPanel({
        lab::noteRow(u"Клавиши -- сетки треугольников с материалом «металличность -- шероховатость»."),
        lab::statusRow(model->sceneStatus),
        lab::statusRow(model->lightStatus),
        lab::group(u"Материал", true,
                   {
                       lab::sliderRow(u"MetallicFactor", model->metallic, 0.0, 1.0, 0.01),
                       lab::sliderRow(u"RoughnessFactor", model->roughness, 0.0, 1.0, 0.01),
                       lab::colorRow(u"BaseColorFactor", model->keyColor),
                   }),
        lab::group(u"Форма клавиши (сетка)", true,
                   {
                       lab::sliderRow(u"Высота над панелью", model->rise, 0.0, 60.0, 0.5),
                       lab::sliderRow(u"Лицо: −1 чаша, +1 купол", model->emboss, -1.0, 1.0, 0.01),
                       lab::sliderRow(u"Ширина плеча", model->shoulder, 0.0, 90.0, 0.5),
                       lab::sliderRow(u"Скругление", model->corner, 0.0, 100.0, 1.0),
                   }),
        lab::group(u"Наклон клавиатуры", true,
                   {
                       lab::sliderRow(u"Вокруг оси x, градусы", model->tiltX, -60.0, 60.0, 1.0),
                       lab::sliderRow(u"Вокруг оси y, градусы", model->tiltY, -60.0, 60.0, 1.0),
                   }),
        lab::group(u"Свет композитора на сцене", true,
                   {
                       lab::choiceRow(u"Источник", model->lightKind, lightKinds),
                       lab::sliderRow(u"Высота лампы над сценой (PointLight)", model->lightHeight, 5.0, 800.0, 1.0),
                       lab::sliderRow(u"Азимут, градусы (DistantLight)", model->azimuth, 0.0, 360.0, 1.0),
                       lab::sliderRow(u"Высота, градусы (DistantLight)", model->elevation, 1.0, 90.0, 1.0),
                       lab::toggleRow(u"Общий свет", model->ambientOn),
                   }),
    });

    return {stage, settings, model};
}
