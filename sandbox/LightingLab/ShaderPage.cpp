// Опыт «Direct3D: свой шейдер».
//
// Клавиша задана формулой: высота над панелью -- функция расстояния до края
// скруглённого прямоугольника (плечо круглое или фаской) и кривизны лица
// (купол или чаша). Пиксельный шейдер берёт нормаль разностью высот, считает
// рассеянный свет и блик, тень клавиши на панели и затенение у её основания --
// то, чего готовые эффекты композитора не дают.
//
// Кадр рисует приложение: шейдер -- в свою текстуру на устройстве Direct3D,
// на котором стоит Direct2D wxl, текстура -- битмапом в поверхность
// композитора XAML этого потока. Рисование идёт на каждое изменение настройки
// и на каждое движение указателя, если лампа идёт за ним.

#include "platform.h"

#include <d2d1_3.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <wrl/client.h>

#include "Lab.h"
#include <wxl/Microsoft.UI.Input.h>
#include <wxl/Microsoft.UI.Xaml.Input.EventArgs.h>

using namespace wxl;
using namespace wxl::dsl;
using Microsoft::WRL::ComPtr;

namespace {

constexpr char16_t const* lightKinds[] = {u"Далёкий", u"Точечный за указателем"};
constexpr char16_t const* shoulders[] = {u"Круглое", u"Фаска"};

// Девять четвёрок: так раскладка одинакова у C++ и у HLSL без правил упаковки.
struct Parameters {
    float frame[4];       // размер кадра и размер ячейки клавиши, пиксели
    float key[4];         // размер клавиши, скругление, ширина плеча
    float shape[4];       // высота клавиши, кривизна лица, фаска, точечная лампа
    float lamp[4];        // направление на лампу или её место; доля общего света
    float lampColor[4];   // цвет света; доля рассеянного
    float keyColor[4];    // цвет клавиши; доля блика
    float panelColor[4];  // цвет панели; показатель блика
    float shade[4];       // сила тени, её мягкость, затенение у основания, счёт в линейном свете
    float origin[4];      // угол первой ячейки; число ячеек в ряду и рядов
};

constexpr std::string_view shaderSource = R"hlsl(
cbuffer Parameters : register(b0) {
    float4 frame;
    float4 key;
    float4 shape;
    float4 lamp;
    float4 lampColor;
    float4 keyColor;
    float4 panelColor;
    float4 shade;
    float4 origin;
};

float4 drawVertex(uint id : SV_VertexID) : SV_Position {
    float2 corner = float2((id << 1) & 2, id & 2);
    return float4(corner * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

// Расстояние до края скруглённого прямоугольника: внутри отрицательное.
float roundBox(float2 p, float2 halfSize, float radius) {
    float2 q = abs(p) - halfSize + radius;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

float2 centreOf(float2 p) {
    float2 cell = clamp(floor((p - origin.xy) / frame.zw), 0.0, origin.zw - 1.0);
    return origin.xy + (cell + 0.5) * frame.zw;
}

float heightAt(float2 p) {
    float2 centre = centreOf(p);
    float inside = -roundBox(p - centre, key.xy * 0.5, key.z);
    if (inside <= 0.0) {
        return 0.0;
    }
    float t = saturate(inside / max(key.w, 0.001));
    float rounded = sqrt(1.0 - (1.0 - t) * (1.0 - t));
    float edge = lerp(rounded, t, shape.z);
    float2 u = (p - centre) / (key.xy * 0.5);
    float bowl = 1.0 - saturate(dot(u, u) * 0.5);
    return shape.x * edge * (1.0 + shape.y * (bowl - 1.0));
}

float3 toLinear(float3 c) { return pow(max(c, 0.0), 2.2); }
float3 toScreen(float3 c) { return pow(max(c, 0.0), 1.0 / 2.2); }

float4 drawPixel(float4 position : SV_Position) : SV_Target {
    float2 p = position.xy;
    float2 centre = centreOf(p);
    float outside = roundBox(p - centre, key.xy * 0.5, key.z);

    float h = heightAt(p);
    float dx = heightAt(p + float2(1.0, 0.0)) - heightAt(p - float2(1.0, 0.0));
    float dy = heightAt(p + float2(0.0, 1.0)) - heightAt(p - float2(0.0, 1.0));
    float3 n = normalize(float3(-dx, -dy, 2.0));

    float3 toLamp = shape.w > 0.5 ? normalize(lamp.xyz - float3(p, h)) : normalize(lamp.xyz);
    float lambert = max(dot(n, toLamp), 0.0);
    float3 halfway = normalize(toLamp + float3(0.0, 0.0, 1.0));
    float gloss = pow(max(dot(n, halfway), 0.0), max(panelColor.w, 1.0)) * keyColor.w;

    // Точка панели в тени, если по пути к лампе, поднявшись на высоту клавиши,
    // луч оказывается над ней.
    float2 reach = toLamp.xy / max(toLamp.z, 0.05) * shape.x;
    float cast = roundBox(p + reach - centre, key.xy * 0.5, key.z);
    float onPanel = step(0.0, outside);
    float open = lerp(1.0, lerp(1.0 - shade.x, 1.0, smoothstep(-shade.y, shade.y, cast)), onPanel);
    float nook = 1.0 - shade.z * exp(-max(outside, 0.0) / max(key.w * 0.5, 1.0)) * onPanel;

    float cover = saturate(0.5 - outside);
    float3 base = lerp(panelColor.rgb, keyColor.rgb, cover);
    float3 light = lampColor.rgb;
    if (shade.w > 0.5) {
        base = toLinear(base);
        light = toLinear(light);
    }
    float3 colour = base * (lamp.w + light * lampColor.w * lambert * open) * nook + light * gloss * open;
    if (shade.w > 0.5) {
        colour = toScreen(colour);
    }
    return float4(saturate(colour), 1.0);
}
)hlsl";

void rgbOf(Color value, float* out) {
    out[0] = value.R / 255.0f;
    out[1] = value.G / 255.0f;
    out[2] = value.B / 255.0f;
}

struct Shaded {
    core::observable<double> rise {14.0};
    core::observable<double> emboss {-0.4};
    core::observable<double> shoulder {18.0};
    core::observable<int> shoulderKind {0};
    core::observable<double> corner {22.0};

    core::observable<int> lightKind {0};
    core::observable<double> azimuth {120.0};
    core::observable<double> elevation {45.0};
    core::observable<double> lightHeight {220.0};
    core::observable<double> ambientShare {0.3};
    core::observable<double> diffuseShare {0.9};
    core::observable<double> specularShare {0.35};
    core::observable<double> shine {48.0};
    core::observable<double> shadow {0.55};
    core::observable<double> softness {5.0};
    core::observable<double> occlusion {0.35};
    core::observable<bool> linearLight {true};
    core::observable<Color> lightColor {rgb(255, 246, 232)};
    core::observable<Color> keyColor {rgb(68, 72, 79)};
    core::observable<Color> panelColor {rgb(42, 44, 49)};

    core::observable<hstring> status;

    Compositor compositor;
    SpriteVisual sprite;
    DrawingSurface surface;
    float scale = 1.0f;
    float lightX = lab::stageWidth * 0.3f;
    float lightY = lab::stageHeight * 0.3f;

    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> immediate;
    ComPtr<ID3D11VertexShader> vertexShader;
    ComPtr<ID3D11PixelShader> pixelShader;
    ComPtr<ID3D11Buffer> constants;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11RenderTargetView> view;
    // Та же текстура глазами Direct2D.
    ComPtr<ID2D1Bitmap1> bitmap;

    Shaded()
        : compositor(CompositionTarget::getCompositorForCurrentThread()),
          sprite(compositor.createSpriteVisual()),
          surface(compositor, SizeInt32 {static_cast<int32_t>(lab::stageWidth), static_cast<int32_t>(lab::stageHeight)}) {
        sprite.size({lab::stageWidth, lab::stageHeight});
        auto const picture = surface.brush();
        picture.stretch(CompositionStretch::Fill);
        sprite.brush(picture);

        lab::onAny([this] { redraw(); }, rise, emboss, shoulder, shoulderKind, corner, lightKind, azimuth, elevation,
                   lightHeight, ambientShare, diffuseShare, specularShare, shine, shadow, softness, occlusion,
                   linearLight, lightColor, keyColor, panelColor);
        redraw();
    }

    void loaded(double rasterizationScale) noexcept {
        scale = static_cast<float>(rasterizationScale);
        surface.resize({static_cast<int32_t>(std::lround(lab::stageWidth * scale)),
                        static_cast<int32_t>(std::lround(lab::stageHeight * scale))});
        texture.Reset();
        view.Reset();
        bitmap.Reset();
        redraw();
    }

    void lightAt(float x, float y) noexcept {
        lightX = x;
        lightY = y;
        if (lightKind.get() != 0) {
            redraw();
        }
    }

    void redraw() noexcept {
        try {
            if (!prepare()) {
                return;
            }
            render();
            surface.draw([this](ID2D1DeviceContext* context) { show(context); });
            status.set(lab::accepted(u"Шейдер"));
        } catch (...) {
            status.set(lab::refusalOfCurrentException(u"Шейдер"));
        }
    }

    // Устройство Direct3D -- то, на котором стоит Direct2D поверхности: иначе
    // её контекст не возьмёт текстуру битмапом.
    bool prepare() {
        if (!device) {
            surface.draw([this](ID2D1DeviceContext* context) {
                ComPtr<ID2D1Device> drawing;
                context->GetDevice(&drawing);
                ComPtr<ID2D1Device2> drawing2;
                wxl::check_hresult(drawing.As(&drawing2));
                ComPtr<IDXGIDevice> dxgi;
                wxl::check_hresult(drawing2->GetDxgiDevice(&dxgi));
                wxl::check_hresult(dxgi.As(&device));
            });
            device->GetImmediateContext(&immediate);
        }

        if (!pixelShader) {
            ComPtr<ID3DBlob> vertexCode;
            ComPtr<ID3DBlob> pixelCode;
            if (!compile("drawVertex", "vs_4_0", vertexCode) || !compile("drawPixel", "ps_4_0", pixelCode)) {
                return false;
            }
            wxl::check_hresult(device->CreateVertexShader(vertexCode->GetBufferPointer(), vertexCode->GetBufferSize(),
                                                          nullptr, &vertexShader));
            wxl::check_hresult(device->CreatePixelShader(pixelCode->GetBufferPointer(), pixelCode->GetBufferSize(),
                                                         nullptr, &pixelShader));

            D3D11_BUFFER_DESC description {};
            description.ByteWidth = sizeof(Parameters);
            description.Usage = D3D11_USAGE_DEFAULT;
            description.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            wxl::check_hresult(device->CreateBuffer(&description, nullptr, &constants));
        }

        if (!texture) {
            SizeInt32 const size = surface.size();
            D3D11_TEXTURE2D_DESC description {};
            description.Width = static_cast<UINT>(size.width);
            description.Height = static_cast<UINT>(size.height);
            description.MipLevels = 1;
            description.ArraySize = 1;
            description.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            description.SampleDesc.Count = 1;
            description.Usage = D3D11_USAGE_DEFAULT;
            description.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            wxl::check_hresult(device->CreateTexture2D(&description, nullptr, &texture));
            wxl::check_hresult(device->CreateRenderTargetView(texture.Get(), nullptr, &view));
            bitmap.Reset();
        }
        return true;
    }

    bool compile(char const* entry, char const* profile, ComPtr<ID3DBlob>& code) {
        ComPtr<ID3DBlob> errors;
        HRESULT const result = D3DCompile(shaderSource.data(), shaderSource.size(), "lighting-lab", nullptr, nullptr,
                                          entry, profile, 0, 0, &code, &errors);
        if (SUCCEEDED(result)) {
            return true;
        }
        // Текст компилятора -- ASCII.
        std::u16string why;
        if (errors) {
            char const* const text = static_cast<char const*>(errors->GetBufferPointer());
            for (std::size_t at = 0; at < errors->GetBufferSize() && text[at] != '\0'; ++at) {
                why.push_back(static_cast<char16_t>(static_cast<unsigned char>(text[at])));
            }
        }
        status.set(lab::refusal(u"D3DCompile", why.c_str()));
        return false;
    }

    void render() {
        SizeInt32 const size = surface.size();
        float const cellWidth = (lab::keySide + lab::keyGap) * scale;

        Parameters p {};
        p.frame[0] = static_cast<float>(size.width);
        p.frame[1] = static_cast<float>(size.height);
        p.frame[2] = cellWidth;
        p.frame[3] = cellWidth;
        p.key[0] = lab::keySide * scale;
        p.key[1] = lab::keySide * scale;
        p.key[2] = static_cast<float>(corner.get()) * scale;
        p.key[3] = static_cast<float>(shoulder.get()) * scale;
        p.shape[0] = static_cast<float>(rise.get()) * scale;
        p.shape[1] = static_cast<float>(emboss.get());
        p.shape[2] = shoulderKind.get() == 1 ? 1.0f : 0.0f;
        p.shape[3] = lightKind.get() == 1 ? 1.0f : 0.0f;
        if (lightKind.get() == 1) {
            p.lamp[0] = lightX * scale;
            p.lamp[1] = lightY * scale;
            p.lamp[2] = static_cast<float>(lightHeight.get()) * scale;
        } else {
            // Лампа под азимутом и высотой: 0 -- справа, 90 -- сверху экрана.
            double const a = azimuth.get() * 3.14159265358979 / 180.0;
            double const e = elevation.get() * 3.14159265358979 / 180.0;
            p.lamp[0] = static_cast<float>(std::cos(a) * std::cos(e));
            p.lamp[1] = static_cast<float>(-std::sin(a) * std::cos(e));
            p.lamp[2] = static_cast<float>(std::sin(e));
        }
        p.lamp[3] = static_cast<float>(ambientShare.get());
        rgbOf(lightColor.get(), p.lampColor);
        p.lampColor[3] = static_cast<float>(diffuseShare.get());
        rgbOf(keyColor.get(), p.keyColor);
        p.keyColor[3] = static_cast<float>(specularShare.get());
        rgbOf(panelColor.get(), p.panelColor);
        p.panelColor[3] = static_cast<float>(shine.get());
        p.shade[0] = static_cast<float>(shadow.get());
        p.shade[1] = static_cast<float>(softness.get()) * scale;
        p.shade[2] = static_cast<float>(occlusion.get());
        p.shade[3] = linearLight.get() ? 1.0f : 0.0f;
        // Ячейка -- клавиша с половиной промежутка по сторонам.
        p.origin[0] = lab::keyGap * scale / 2.0f;
        p.origin[1] = lab::keyGap * scale / 2.0f;
        p.origin[2] = static_cast<float>(lab::keyColumns);
        p.origin[3] = static_cast<float>(lab::keyRows);

        immediate->UpdateSubresource(constants.Get(), 0, nullptr, &p, 0, 0);

        D3D11_VIEWPORT const viewport {0.0f, 0.0f, static_cast<float>(size.width), static_cast<float>(size.height),
                                       0.0f, 1.0f};
        ID3D11RenderTargetView* const targets[] = {view.Get()};
        ID3D11Buffer* const buffers[] = {constants.Get()};
        immediate->OMSetRenderTargets(1, targets, nullptr);
        immediate->RSSetViewports(1, &viewport);
        immediate->IASetInputLayout(nullptr);
        immediate->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        immediate->VSSetShader(vertexShader.Get(), nullptr, 0);
        immediate->PSSetShader(pixelShader.Get(), nullptr, 0);
        immediate->PSSetConstantBuffers(0, 1, buffers);
        immediate->Draw(3, 0);
        // Текстуру дальше читает Direct2D: целью она оставаться не должна.
        immediate->OMSetRenderTargets(0, nullptr, nullptr);
    }

    void show(ID2D1DeviceContext* context) {
        SizeInt32 const size = surface.size();
        if (!bitmap) {
            ComPtr<IDXGISurface> dxgi;
            wxl::check_hresult(texture.As(&dxgi));
            D2D1_BITMAP_PROPERTIES1 const properties = D2D1::BitmapProperties1(
                D2D1_BITMAP_OPTIONS_NONE,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
            wxl::check_hresult(context->CreateBitmapFromDxgiSurface(dxgi.Get(), &properties, &bitmap));
        }
        context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
        context->DrawBitmap(bitmap.Get(),
                            D2D1::RectF(0.0f, 0.0f, static_cast<float>(size.width), static_cast<float>(size.height)),
                            1.0f, D2D1_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
    }
};

}  // namespace

lab::Experiment lab::shaderPage() {
    auto const model = std::make_shared<Shaded>();

    auto stage = Border {
        width = double {lab::stageWidth},
        height = double {lab::stageHeight},
        hAlign.center,
        vAlign.center,
        background = SolidColorBrush {color = rgb(24, 25, 28)},
        [model](Border const& host) { ElementCompositionPreview::setElementChildVisual(host, model->sprite); },
        onLoaded = [model](Border const& self) { model->loaded(self.xamlRoot().rasterizationScale()); },
        onPointerMoved =
            [model](Border const& self, PointerRoutedEventArgs& args) {
                Point const place = args.getCurrentPoint(self).position();
                model->lightAt(place.x, place.y);
            },
    };

    auto settings = lab::settingsPanel({
        lab::noteRow(u"Клавиша -- формула: высота от расстояния до края и кривизны лица."),
        lab::statusRow(model->status),
        lab::group(u"Форма клавиши", true,
                   {
                       lab::sliderRow(u"Высота над панелью", model->rise, 0.0, 60.0, 0.5),
                       lab::sliderRow(u"Лицо: −1 чаша, +1 купол", model->emboss, -1.0, 1.0, 0.01),
                       lab::sliderRow(u"Ширина плеча", model->shoulder, 0.0, 90.0, 0.5),
                       lab::choiceRow(u"Плечо", model->shoulderKind, shoulders),
                       lab::sliderRow(u"Скругление", model->corner, 0.0, 100.0, 1.0),
                       lab::colorRow(u"Цвет клавиш", model->keyColor),
                       lab::colorRow(u"Цвет панели", model->panelColor),
                   }),
        lab::group(u"Свет", true,
                   {
                       lab::choiceRow(u"Лампа", model->lightKind, lightKinds),
                       lab::sliderRow(u"Азимут, градусы (далёкий)", model->azimuth, 0.0, 360.0, 1.0),
                       lab::sliderRow(u"Высота, градусы (далёкий)", model->elevation, 1.0, 90.0, 1.0),
                       lab::sliderRow(u"Высота лампы над сценой (точечный)", model->lightHeight, 5.0, 800.0, 1.0),
                       lab::sliderRow(u"Общий свет", model->ambientShare, 0.0, 1.0, 0.01),
                       lab::sliderRow(u"Рассеянный свет", model->diffuseShare, 0.0, 3.0, 0.01),
                       lab::sliderRow(u"Блик", model->specularShare, 0.0, 3.0, 0.01),
                       lab::sliderRow(u"Показатель блика", model->shine, 1.0, 256.0, 1.0),
                       lab::toggleRow(u"Считать в линейном свете", model->linearLight),
                       lab::colorRow(u"Цвет света", model->lightColor),
                   }),
        lab::group(u"Тень и затенение", true,
                   {
                       lab::sliderRow(u"Сила тени клавиши на панели", model->shadow, 0.0, 1.0, 0.01),
                       lab::sliderRow(u"Мягкость края тени", model->softness, 0.5, 40.0, 0.5),
                       lab::sliderRow(u"Затенение у основания", model->occlusion, 0.0, 1.0, 0.01),
                   }),
    });

    return {stage, settings, model};
}
