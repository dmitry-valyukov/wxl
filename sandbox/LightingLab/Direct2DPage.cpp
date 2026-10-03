// Опыт «Direct2D: освещение по карте высот».
//
// Встроенные эффекты освещения Direct2D читают высоту из альфа-канала входа и
// берут нормаль оператором Собеля. Здесь высота -- картинка клавиш: форма
// закрашена альфой, край размыт, цифра вдавлена. Свет умножается на картинку
// цветов (панель и клавиши), блик складывается сверху.
//
// Весь кадр рисует приложение в поверхность композитора XAML этого потока, на
// каждое изменение настройки и на каждое движение указателя, если лампа идёт
// за ним.

#include "platform.h"

// dwrite.h первым: он приводит guiddef.h с DEFINE_GUID, без которого
// d2d1effects.h не раскрывает CLSID эффектов.
#include <dwrite.h>

#include <d2d1_1.h>
#include <d2d1effects.h>
#include <wrl/client.h>

#include "Lab.h"
#include "generated/Microsoft.UI.Input.h"
#include "generated/Microsoft.UI.Xaml.Input.EventArgs.h"

using namespace wxl;
using namespace wxl::dsl;
using Microsoft::WRL::ComPtr;

namespace {

constexpr char16_t const* lightKinds[] = {u"Далёкий (Distant)", u"Точечный за указателем (Point)",
                                          u"Прожектор за указателем (Spot)"};
constexpr char16_t const* profiles[] = {u"Плоская с кромкой", u"Купол", u"Чаша"};
constexpr wchar_t const* digits[] = {L"7", L"8", L"9", L"4", L"5", L"6"};

D2D1_COLOR_F toColor(Color value, float alpha = 1.0f) {
    return D2D1::ColorF(value.R / 255.0f, value.G / 255.0f, value.B / 255.0f, alpha);
}

D2D1_VECTOR_3F toVector(Color value) {
    return D2D1::Vector3F(value.R / 255.0f, value.G / 255.0f, value.B / 255.0f);
}

struct Carving {
    core::observable<int> lightKind {0};
    core::observable<bool> diffuseOn {true};
    core::observable<bool> specularOn {true};
    core::observable<double> ambientShare {0.25};
    core::observable<double> diffuseConstant {1.0};
    core::observable<double> specularConstant {0.5};
    core::observable<double> specularExponent {24.0};
    core::observable<double> azimuth {120.0};
    core::observable<double> elevation {40.0};
    core::observable<double> lightHeight {180.0};
    core::observable<double> focus {4.0};
    core::observable<double> cone {50.0};
    core::observable<Color> lightColor {rgb(255, 246, 232)};

    core::observable<int> profile {0};
    core::observable<double> softness {5.0};
    core::observable<double> surfaceScale {10.0};
    core::observable<double> kernel {1.0};
    core::observable<double> corner {22.0};
    core::observable<bool> engraved {true};
    core::observable<Color> keyColor {rgb(68, 72, 79)};
    core::observable<Color> panelColor {rgb(42, 44, 49)};

    core::observable<hstring> status;

    Compositor compositor;
    SpriteVisual sprite;
    DrawingSurface surface;
    ComPtr<IDWriteTextFormat> digitFormat;
    float scale = 1.0f;
    // Место лампы над сценой, DIP: туда, где был указатель.
    float lightX = lab::stageWidth * 0.3f;
    float lightY = lab::stageHeight * 0.3f;

    Carving()
        : compositor(CompositionTarget::getCompositorForCurrentThread()),
          sprite(compositor.createSpriteVisual()),
          surface(compositor, SizeInt32 {static_cast<int32_t>(lab::stageWidth), static_cast<int32_t>(lab::stageHeight)}) {
        sprite.size({lab::stageWidth, lab::stageHeight});
        auto const picture = surface.brush();
        picture.stretch(CompositionStretch::Fill);
        sprite.brush(picture);

        ComPtr<IDWriteFactory> factory;
        if (SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                          reinterpret_cast<IUnknown**>(factory.GetAddressOf()))) &&
            SUCCEEDED(factory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                                                DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 96.0f, L"",
                                                &digitFormat))) {
            digitFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            digitFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        }

        lab::onAny([this] { redraw(); }, lightKind, diffuseOn, specularOn, ambientShare, diffuseConstant,
                   specularConstant, specularExponent, azimuth, elevation, lightHeight, focus, cone, lightColor,
                   profile, softness, surfaceScale, kernel, corner, engraved, keyColor, panelColor);
        redraw();
    }

    // Элемент на экране: теперь известен масштаб, поверхность -- в настоящих пикселях.
    void loaded(double rasterizationScale) noexcept {
        scale = static_cast<float>(rasterizationScale);
        surface.resize({static_cast<int32_t>(std::lround(lab::stageWidth * scale)),
                        static_cast<int32_t>(std::lround(lab::stageHeight * scale))});
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
        lab::attempt(status, u"Эффекты освещения Direct2D", [this] {
            surface.draw([this](ID2D1DeviceContext* context) { paint(context); });
        });
    }

    D2D1_ROUNDED_RECT keyOutline(int index) const {
        float const left = lab::keyLeft(index % lab::keyColumns), top = lab::keyTop(index / lab::keyColumns);
        float const radius = static_cast<float>(corner.get());
        return {D2D1::RectF(left, top, left + lab::keySide, top + lab::keySide), radius, radius};
    }

    // Высота в альфе: форма клавиши целиком или с покатым лицом, цифра -- ниже лица.
    void drawHeights(ID2D1DeviceContext* side) {
        side->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
        side->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);

        ComPtr<ID2D1SolidColorBrush> full;
        ComPtr<ID2D1SolidColorBrush> sunk;
        wxl::check_hresult(side->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f), &full));
        wxl::check_hresult(side->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.6f), &sunk));

        for (int index = 0; index < lab::keyColumns * lab::keyRows; ++index) {
            D2D1_ROUNDED_RECT const outline = keyOutline(index);
            ID2D1Brush* face = full.Get();

            ComPtr<ID2D1RadialGradientBrush> slope;
            if (profile.get() != 0) {
                // Купол выше в середине, чаша -- у края.
                bool const dome = profile.get() == 1;
                D2D1_GRADIENT_STOP const stops[] = {
                    {0.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, dome ? 1.0f : 0.45f)},
                    {1.0f, D2D1::ColorF(1.0f, 1.0f, 1.0f, dome ? 0.45f : 1.0f)},
                };
                ComPtr<ID2D1GradientStopCollection> collection;
                wxl::check_hresult(side->CreateGradientStopCollection(stops, 2, &collection));
                D2D1_POINT_2F const centre = D2D1::Point2F((outline.rect.left + outline.rect.right) / 2.0f,
                                                            (outline.rect.top + outline.rect.bottom) / 2.0f);
                wxl::check_hresult(side->CreateRadialGradientBrush(
                    D2D1::RadialGradientBrushProperties(centre, D2D1::Point2F(), lab::keySide * 0.72f,
                                                        lab::keySide * 0.72f),
                    collection.Get(), &slope));
                face = slope.Get();
            }
            side->FillRoundedRectangle(outline, face);

            if (engraved.get() && digitFormat) {
                // Копией: альфа цифры заменяет альфу лица, а не ложится поверх.
                side->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
                side->DrawText(digits[index], 1, digitFormat.Get(), outline.rect, sunk.Get());
                side->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_SOURCE_OVER);
            }
        }
    }

    void drawColors(ID2D1DeviceContext* side) {
        side->Clear(toColor(panelColor.get()));
        ComPtr<ID2D1SolidColorBrush> key;
        wxl::check_hresult(side->CreateSolidColorBrush(toColor(keyColor.get()), &key));
        for (int index = 0; index < lab::keyColumns * lab::keyRows; ++index) {
            side->FillRoundedRectangle(keyOutline(index), key.Get());
        }
    }

    void paint(ID2D1DeviceContext* context) {
        SizeInt32 const size = surface.size();
        D2D1_SIZE_U const pixels = D2D1::SizeU(static_cast<UINT32>(size.width), static_cast<UINT32>(size.height));

        // Две картинки рисуются своим контекстом того же устройства: контекст
        // поверхности занят её кадром.
        ComPtr<ID2D1Device> device;
        context->GetDevice(&device);
        ComPtr<ID2D1DeviceContext> side;
        wxl::check_hresult(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &side));

        D2D1_BITMAP_PROPERTIES1 const targetProperties = D2D1::BitmapProperties1(
            D2D1_BITMAP_OPTIONS_TARGET, D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        ComPtr<ID2D1Bitmap1> heights;
        ComPtr<ID2D1Bitmap1> colors;
        wxl::check_hresult(side->CreateBitmap(pixels, nullptr, 0, &targetProperties, &heights));
        wxl::check_hresult(side->CreateBitmap(pixels, nullptr, 0, &targetProperties, &colors));

        side->SetTarget(heights.Get());
        side->BeginDraw();
        side->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale));
        drawHeights(side.Get());
        wxl::check_hresult(side->EndDraw());

        side->SetTarget(colors.Get());
        side->BeginDraw();
        side->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale));
        drawColors(side.Get());
        wxl::check_hresult(side->EndDraw());
        side->SetTarget(nullptr);

        auto const make = [&](REFCLSID id) {
            ComPtr<ID2D1Effect> effect;
            wxl::check_hresult(context->CreateEffect(id, &effect));
            return effect;
        };

        // Размытие края -- ширина кромки: чем шире переход альфы, тем положе склон.
        auto const blur = make(CLSID_D2D1GaussianBlur);
        blur->SetInput(0, heights.Get());
        blur->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, static_cast<float>(softness.get()) * scale);
        blur->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_HARD);

        int const kind = lightKind.get();
        D2D1_VECTOR_3F const lamp = D2D1::Vector3F(lightX * scale, lightY * scale,
                                                    static_cast<float>(lightHeight.get()) * scale);
        D2D1_VECTOR_3F const aimedAt = D2D1::Vector3F(lightX * scale, lightY * scale, 0.0f);
        D2D1_VECTOR_3F const tint = toVector(lightColor.get());
        float const relief = static_cast<float>(surfaceScale.get()) * scale;
        D2D1_VECTOR_2F const unit = D2D1::Vector2F(static_cast<float>(kernel.get()) * scale,
                                                    static_cast<float>(kernel.get()) * scale);
        // Direct2D ведёт азимут от оси x к оси y, а она на экране смотрит вниз: лампа,
        // стоящая под углом сверху экрана, для него стоит под обратным углом.
        float const turn = 360.0f - static_cast<float>(azimuth.get());
        float const rise = static_cast<float>(elevation.get());

        auto const diffuse = [&] {
            float const share = static_cast<float>(diffuseConstant.get());
            if (kind == 0) {
                auto const effect = make(CLSID_D2D1DistantDiffuse);
                effect->SetValue(D2D1_DISTANTDIFFUSE_PROP_AZIMUTH, turn);
                effect->SetValue(D2D1_DISTANTDIFFUSE_PROP_ELEVATION, rise);
                effect->SetValue(D2D1_DISTANTDIFFUSE_PROP_DIFFUSE_CONSTANT, share);
                effect->SetValue(D2D1_DISTANTDIFFUSE_PROP_SURFACE_SCALE, relief);
                effect->SetValue(D2D1_DISTANTDIFFUSE_PROP_COLOR, tint);
                effect->SetValue(D2D1_DISTANTDIFFUSE_PROP_KERNEL_UNIT_LENGTH, unit);
                return effect;
            }
            if (kind == 1) {
                auto const effect = make(CLSID_D2D1PointDiffuse);
                effect->SetValue(D2D1_POINTDIFFUSE_PROP_LIGHT_POSITION, lamp);
                effect->SetValue(D2D1_POINTDIFFUSE_PROP_DIFFUSE_CONSTANT, share);
                effect->SetValue(D2D1_POINTDIFFUSE_PROP_SURFACE_SCALE, relief);
                effect->SetValue(D2D1_POINTDIFFUSE_PROP_COLOR, tint);
                effect->SetValue(D2D1_POINTDIFFUSE_PROP_KERNEL_UNIT_LENGTH, unit);
                return effect;
            }
            auto const effect = make(CLSID_D2D1SpotDiffuse);
            effect->SetValue(D2D1_SPOTDIFFUSE_PROP_LIGHT_POSITION, lamp);
            effect->SetValue(D2D1_SPOTDIFFUSE_PROP_POINTS_AT, aimedAt);
            effect->SetValue(D2D1_SPOTDIFFUSE_PROP_FOCUS, static_cast<float>(focus.get()));
            effect->SetValue(D2D1_SPOTDIFFUSE_PROP_LIMITING_CONE_ANGLE, static_cast<float>(cone.get()));
            effect->SetValue(D2D1_SPOTDIFFUSE_PROP_DIFFUSE_CONSTANT, share);
            effect->SetValue(D2D1_SPOTDIFFUSE_PROP_SURFACE_SCALE, relief);
            effect->SetValue(D2D1_SPOTDIFFUSE_PROP_COLOR, tint);
            effect->SetValue(D2D1_SPOTDIFFUSE_PROP_KERNEL_UNIT_LENGTH, unit);
            return effect;
        };

        auto const specular = [&] {
            float const share = static_cast<float>(specularConstant.get());
            float const exponent = static_cast<float>(specularExponent.get());
            if (kind == 0) {
                auto const effect = make(CLSID_D2D1DistantSpecular);
                effect->SetValue(D2D1_DISTANTSPECULAR_PROP_AZIMUTH, turn);
                effect->SetValue(D2D1_DISTANTSPECULAR_PROP_ELEVATION, rise);
                effect->SetValue(D2D1_DISTANTSPECULAR_PROP_SPECULAR_EXPONENT, exponent);
                effect->SetValue(D2D1_DISTANTSPECULAR_PROP_SPECULAR_CONSTANT, share);
                effect->SetValue(D2D1_DISTANTSPECULAR_PROP_SURFACE_SCALE, relief);
                effect->SetValue(D2D1_DISTANTSPECULAR_PROP_COLOR, tint);
                effect->SetValue(D2D1_DISTANTSPECULAR_PROP_KERNEL_UNIT_LENGTH, unit);
                return effect;
            }
            if (kind == 1) {
                auto const effect = make(CLSID_D2D1PointSpecular);
                effect->SetValue(D2D1_POINTSPECULAR_PROP_LIGHT_POSITION, lamp);
                effect->SetValue(D2D1_POINTSPECULAR_PROP_SPECULAR_EXPONENT, exponent);
                effect->SetValue(D2D1_POINTSPECULAR_PROP_SPECULAR_CONSTANT, share);
                effect->SetValue(D2D1_POINTSPECULAR_PROP_SURFACE_SCALE, relief);
                effect->SetValue(D2D1_POINTSPECULAR_PROP_COLOR, tint);
                effect->SetValue(D2D1_POINTSPECULAR_PROP_KERNEL_UNIT_LENGTH, unit);
                return effect;
            }
            auto const effect = make(CLSID_D2D1SpotSpecular);
            effect->SetValue(D2D1_SPOTSPECULAR_PROP_LIGHT_POSITION, lamp);
            effect->SetValue(D2D1_SPOTSPECULAR_PROP_POINTS_AT, aimedAt);
            effect->SetValue(D2D1_SPOTSPECULAR_PROP_FOCUS, static_cast<float>(focus.get()));
            effect->SetValue(D2D1_SPOTSPECULAR_PROP_LIMITING_CONE_ANGLE, static_cast<float>(cone.get()));
            effect->SetValue(D2D1_SPOTSPECULAR_PROP_SPECULAR_EXPONENT, exponent);
            effect->SetValue(D2D1_SPOTSPECULAR_PROP_SPECULAR_CONSTANT, share);
            effect->SetValue(D2D1_SPOTSPECULAR_PROP_SURFACE_SCALE, relief);
            effect->SetValue(D2D1_SPOTSPECULAR_PROP_COLOR, tint);
            effect->SetValue(D2D1_SPOTSPECULAR_PROP_KERNEL_UNIT_LENGTH, unit);
            return effect;
        };

        // Цвет × (общий свет + рассеянный): произведение двух входов и доля первого.
        float const ambient = static_cast<float>(ambientShare.get());
        auto const lit = make(CLSID_D2D1ArithmeticComposite);
        lit->SetInput(0, colors.Get());
        if (diffuseOn.get()) {
            auto const light = diffuse();
            light->SetInputEffect(0, blur.Get());
            lit->SetInputEffect(1, light.Get());
            lit->SetValue(D2D1_ARITHMETICCOMPOSITE_PROP_COEFFICIENTS, D2D1::Vector4F(1.0f, ambient, 0.0f, 0.0f));
        } else {
            lit->SetInput(1, colors.Get());
            lit->SetValue(D2D1_ARITHMETICCOMPOSITE_PROP_COEFFICIENTS, D2D1::Vector4F(0.0f, ambient, 0.0f, 0.0f));
        }

        ComPtr<ID2D1Effect> picture = lit;
        if (specularOn.get()) {
            auto const gloss = specular();
            gloss->SetInputEffect(0, blur.Get());
            auto const sum = make(CLSID_D2D1Composite);
            sum->SetValue(D2D1_COMPOSITE_PROP_MODE, D2D1_COMPOSITE_MODE_PLUS);
            sum->SetInputEffect(0, lit.Get());
            sum->SetInputEffect(1, gloss.Get());
            picture = sum;
        }

        // Поверхность -- часть общего атласа: без обрезки вывод эффекта, который
        // шире картинки, лёг бы на соседей.
        context->PushAxisAlignedClip(D2D1::RectF(0.0f, 0.0f, static_cast<float>(size.width),
                                                 static_cast<float>(size.height)),
                                     D2D1_ANTIALIAS_MODE_ALIASED);
        context->DrawImage(picture.Get());
        context->PopAxisAlignedClip();
    }
};

}  // namespace

lab::Experiment lab::direct2dPage() {
    auto const model = std::make_shared<Carving>();

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
        lab::noteRow(u"Высота -- альфа картинки клавиш с размытым краем; панель и клавиши -- один рельеф."),
        lab::statusRow(model->status),
        lab::group(u"Свет", true,
                   {
                       lab::choiceRow(u"Источник", model->lightKind, lightKinds),
                       lab::toggleRow(u"Рассеянный свет (Diffuse)", model->diffuseOn),
                       lab::toggleRow(u"Блик (Specular)", model->specularOn),
                       lab::sliderRow(u"Общий свет, доля цвета", model->ambientShare, 0.0, 1.0, 0.01),
                       lab::sliderRow(u"DiffuseConstant", model->diffuseConstant, 0.0, 4.0, 0.01),
                       lab::sliderRow(u"SpecularConstant", model->specularConstant, 0.0, 4.0, 0.01),
                       lab::sliderRow(u"SpecularExponent", model->specularExponent, 1.0, 128.0, 1.0),
                       lab::sliderRow(u"Азимут, градусы (Distant)", model->azimuth, 0.0, 360.0, 1.0),
                       lab::sliderRow(u"Высота, градусы (Distant)", model->elevation, 0.0, 90.0, 1.0),
                       lab::sliderRow(u"Высота лампы над сценой (Point, Spot)", model->lightHeight, 5.0, 600.0, 1.0),
                       lab::sliderRow(u"Focus (Spot)", model->focus, 0.0, 200.0, 1.0),
                       lab::sliderRow(u"LimitingConeAngle, градусы (Spot)", model->cone, 0.0, 90.0, 1.0),
                       lab::colorRow(u"Цвет света", model->lightColor),
                   }),
        lab::group(u"Рельеф (карта высот)", true,
                   {
                       lab::choiceRow(u"Лицо клавиши", model->profile, profiles),
                       lab::sliderRow(u"Ширина кромки: размытие высоты", model->softness, 0.0, 30.0, 0.5),
                       lab::sliderRow(u"SurfaceScale: высота рельефа", model->surfaceScale, 0.0, 60.0, 0.5),
                       lab::sliderRow(u"KernelUnitLength", model->kernel, 0.5, 8.0, 0.5),
                       lab::sliderRow(u"Скругление", model->corner, 0.0, 100.0, 1.0),
                       lab::toggleRow(u"Вдавленная цифра", model->engraved),
                       lab::colorRow(u"Цвет клавиш", model->keyColor),
                       lab::colorRow(u"Цвет панели", model->panelColor),
                   }),
    });

    return {stage, settings, model};
}
