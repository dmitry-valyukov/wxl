// Опыт «Системный композитор».
//
// Две проверки на композиторе самой Windows (API Windows.UI.Composition), а не
// на отделённом Microsoft.UI.Composition, которым рисует XAML:
//
// 1. Карта нормалей и граница спрайта. Сцена образца ThumbnailLighting: карты
//    образца, его граф, далёкий свет строго справа. Каждая карта стоит дважды,
//    с мягкой и с жёсткой границей спрайта. На отделённом композиторе мягкая
//    граница поворачивает ответ карты на свет; здесь видно, так ли на системном.
// 2. Шесть эффектов освещения Win2D в фабрике эффектов: что отвечает системная
//    фабрика и что отделённая на вход-параметр, на вход-эффект, на эффект без
//    входа и на анимируемое свойство его лампы. Что эффект рисует без входа,
//    показано плитками в окне.
//
// Системному композитору нужна цель -- голое окно: у опыта своё окно Win32, а
// страница стенда показывает ответы фабрик текстом. Очередь Windows.System на
// потоке уже есть: её заводит цикл wxl.

#include "platform.h"

#include <winrt/Microsoft.Graphics.Canvas.Effects.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Windows.UI.Composition.Desktop.h>
#include <winrt/Windows.UI.Composition.Effects.h>
#include <winrt/Windows.UI.Composition.h>

#include <d2d1_1.h>
#include <d3d11.h>
#include <dxgi.h>
#include <windows.ui.composition.interop.h>

#include <functional>
#include <string>
#include <vector>

#include "Lab.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

namespace sys = winrt::Windows::UI::Composition;
namespace lifted = winrt::Microsoft::UI::Composition;
namespace canvas = winrt::Microsoft::Graphics::Canvas;
namespace effects = winrt::Microsoft::Graphics::Canvas::Effects;
namespace directx = winrt::Windows::Graphics::DirectX;
using winrt::Windows::Graphics::Effects::IGraphicsEffect;
using winrt::Windows::Graphics::Effects::IGraphicsEffectSource;

wchar_t const windowClass[] = L"wxl.LightingLab.SystemCompositor";
wchar_t const windowTitle[] = L"wxl — проба системного композитора";

constexpr float tile = 200.0f;
constexpr float gap = 20.0f;
constexpr int clientWidth = 900;
constexpr int clientHeight = 680;

ATOM registered() {
    static ATOM const atom = [] {
        WNDCLASSEXW description {};
        description.cbSize = sizeof description;
        description.lpfnWndProc = &::DefWindowProcW;
        description.hInstance = ::GetModuleHandleW(nullptr);
        description.lpszClassName = windowClass;
        return ::RegisterClassExW(&description);
    }();
    return atom;
}

// Эффект освещения Win2D: имя, описание с данным входом и свойство лампы,
// которое пробуется анимируемым.
struct Recipe {
    char16_t const* name;
    std::function<IGraphicsEffect(IGraphicsEffectSource const&)> make;
    wchar_t const* animated;
};

template <class Effect>
Effect named(IGraphicsEffectSource const& source) {
    Effect effect;
    effect.Name(L"Light");
    if (source) {
        effect.Source(source);
    }
    return effect;
}

// Лампа у всех справа сверху от середины плитки; далёкая -- строго справа,
// на высоте 30 градусов.
std::vector<Recipe> recipes() {
    constexpr float lift = 0.5236f;
    return {
        {u"DistantDiffuseEffect",
         [](IGraphicsEffectSource const& source) {
             auto effect = named<effects::DistantDiffuseEffect>(source);
             effect.Azimuth(0.0f);
             effect.Elevation(lift);
             return effect.as<IGraphicsEffect>();
         },
         L"Light.Azimuth"},
        {u"PointDiffuseEffect",
         [](IGraphicsEffectSource const& source) {
             auto effect = named<effects::PointDiffuseEffect>(source);
             effect.LightPosition({160.0f, 40.0f, 60.0f});
             return effect.as<IGraphicsEffect>();
         },
         L"Light.LightPosition"},
        {u"SpotDiffuseEffect",
         [](IGraphicsEffectSource const& source) {
             auto effect = named<effects::SpotDiffuseEffect>(source);
             effect.LightPosition({160.0f, 40.0f, 100.0f});
             effect.LightTarget({100.0f, 100.0f, 0.0f});
             effect.Focus(1.0f);
             effect.LimitingConeAngle(1.2f);
             return effect.as<IGraphicsEffect>();
         },
         L"Light.LightPosition"},
        {u"DistantSpecularEffect",
         [](IGraphicsEffectSource const& source) {
             auto effect = named<effects::DistantSpecularEffect>(source);
             effect.Azimuth(0.0f);
             effect.Elevation(lift);
             effect.SpecularExponent(8.0f);
             return effect.as<IGraphicsEffect>();
         },
         L"Light.Azimuth"},
        {u"PointSpecularEffect",
         [](IGraphicsEffectSource const& source) {
             auto effect = named<effects::PointSpecularEffect>(source);
             effect.LightPosition({160.0f, 40.0f, 60.0f});
             effect.SpecularExponent(8.0f);
             return effect.as<IGraphicsEffect>();
         },
         L"Light.LightPosition"},
        {u"SpotSpecularEffect",
         [](IGraphicsEffectSource const& source) {
             auto effect = named<effects::SpotSpecularEffect>(source);
             effect.LightPosition({160.0f, 40.0f, 100.0f});
             effect.LightTarget({100.0f, 100.0f, 0.0f});
             effect.Focus(1.0f);
             effect.LimitingConeAngle(1.2f);
             effect.SpecularExponent(8.0f);
             return effect.as<IGraphicsEffect>();
         },
         L"Light.LightPosition"},
    };
}

struct System {
    core::observable<hstring> report;

    HWND window = nullptr;
    sys::Compositor compositor {nullptr};
    sys::Desktop::DesktopWindowTarget target {nullptr};
    sys::CompositionGraphicsDevice graphics {nullptr};
    sys::ContainerVisual root {nullptr};
    sys::DistantLight lamp {nullptr};
    std::u16string text;

    System() {
        try {
            open();
            line(u"Окно на системном Windows.UI.Composition открыто.");
            normalMapTiles();
            lightingEffects();
        } catch (winrt::hresult_error const& error) {
            refused(u"Сцена системного композитора", error);
        }
        report.set(hstring {text});
    }

    ~System() {
        target = nullptr;
        if (window) {
            ::DestroyWindow(window);
        }
    }

    void line(std::u16string const& words) {
        text += words;
        text += u"\n";
    }

    void refused(std::u16string const& what, winrt::hresult_error const& error) {
        std::u16string words = what;
        words += u": отказ 0x";
        core::append_number(words, static_cast<std::uint32_t>(error.code().value), 16);
        winrt::hstring const message = error.message();
        if (!message.empty()) {
            words += u", ";
            for (wchar_t const unit : message) {
                if (unit != L'\r' && unit != L'\n') {
                    words.push_back(static_cast<char16_t>(unit));
                }
            }
        }
        line(words);
    }

    void open() {
        registered();
        RECT frame {0, 0, clientWidth, clientHeight};
        ::AdjustWindowRectEx(&frame, WS_OVERLAPPEDWINDOW, FALSE, WS_EX_NOREDIRECTIONBITMAP);
        window = ::CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP, windowClass, windowTitle,
                                   WS_OVERLAPPEDWINDOW | WS_VISIBLE, 60, 60, frame.right - frame.left,
                                   frame.bottom - frame.top, nullptr, nullptr, ::GetModuleHandleW(nullptr), nullptr);
        winrt::check_bool(window != nullptr);

        compositor = sys::Compositor {};

        auto const desktop = compositor.as<ABI::Windows::UI::Composition::Desktop::ICompositorDesktopInterop>();
        ABI::Windows::UI::Composition::Desktop::IDesktopWindowTarget* raw = nullptr;
        winrt::check_hresult(desktop->CreateDesktopWindowTarget(window, true, &raw));
        winrt::attach_abi(target, raw);

        root = compositor.CreateContainerVisual();
        root.RelativeSizeAdjustment({1.0f, 1.0f});
        target.Root(root);

        auto const floor = compositor.CreateSpriteVisual();
        floor.RelativeSizeAdjustment({1.0f, 1.0f});
        floor.Brush(compositor.CreateColorBrush({255, 30, 31, 34}));
        root.Children().InsertAtBottom(floor);

        // Четыре звена от Direct2D до поверхности композитора -- свои, против
        // этого композитора: поверхности отделённого ему чужие.
        winrt::com_ptr<ID3D11Device> d3d;
        winrt::check_hresult(::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                                 D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
                                                 d3d.put(), nullptr, nullptr));
        D2D1_CREATION_PROPERTIES const properties {D2D1_THREADING_MODE_SINGLE_THREADED, D2D1_DEBUG_LEVEL_NONE,
                                                   D2D1_DEVICE_CONTEXT_OPTIONS_NONE};
        winrt::com_ptr<ID2D1Device> d2d;
        winrt::check_hresult(::D2D1CreateDevice(d3d.as<IDXGIDevice>().get(), &properties, d2d.put()));

        auto const interop = compositor.as<ABI::Windows::UI::Composition::ICompositorInterop>();
        ABI::Windows::UI::Composition::ICompositionGraphicsDevice* device = nullptr;
        winrt::check_hresult(interop->CreateGraphicsDevice(d2d.as<::IUnknown>().get(), &device));
        winrt::attach_abi(graphics, device);
    }

    // Пиксели BGRA с умноженной альфой -- в поверхность системного композитора.
    sys::CompositionSurfaceBrush brushOf(lab::Pixels const& image) {
        auto const surface = graphics.CreateDrawingSurface(
            {static_cast<float>(image.width), static_cast<float>(image.height)},
            directx::DirectXPixelFormat::B8G8R8A8UIntNormalized, directx::DirectXAlphaMode::Premultiplied);

        auto const interop = surface.as<ABI::Windows::UI::Composition::ICompositionDrawingSurfaceInterop>();
        winrt::com_ptr<ID2D1DeviceContext> context;
        POINT offset {};
        winrt::check_hresult(
            interop->BeginDraw(nullptr, winrt::guid_of<ID2D1DeviceContext>(), context.put_void(), &offset));
        context->SetTransform(
            D2D1::Matrix3x2F::Translation(static_cast<float>(offset.x), static_cast<float>(offset.y)));
        context->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

        D2D1_BITMAP_PROPERTIES const format {{DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED}, 96.0f,
                                             96.0f};
        winrt::com_ptr<ID2D1Bitmap> bitmap;
        winrt::check_hresult(context->CreateBitmap(
            D2D1::SizeU(static_cast<UINT32>(image.width), static_cast<UINT32>(image.height)), image.pixels.data(),
            static_cast<UINT32>(image.width) * 4, format, bitmap.put()));
        context->SetPrimitiveBlend(D2D1_PRIMITIVE_BLEND_COPY);
        context->DrawBitmap(
            bitmap.get(),
            D2D1::RectF(0.0f, 0.0f, static_cast<float>(image.width), static_cast<float>(image.height)), 1.0f,
            D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR);
        winrt::check_hresult(interop->EndDraw());

        auto const brush = compositor.CreateSurfaceBrush(surface);
        brush.Stretch(sys::CompositionStretch::Fill);
        return brush;
    }

    sys::SpriteVisual tileAt(int column, int row) {
        auto const sprite = compositor.CreateSpriteVisual();
        sprite.Size({tile, tile});
        sprite.Offset({gap + static_cast<float>(column) * (tile + gap), gap + static_cast<float>(row) * (tile + gap),
                       0.0f});
        return sprite;
    }

    // Плитки с картами образца под далёким светом строго справа; граф --
    // DistantDiffuse образца ThumbnailLighting. Граница спрайта у плиток разная,
    // и записанные свойства тут же читаются обратно: ответ на вопрос, не стоит
    // ли за поворотом света значение по умолчанию или несостоявшаяся запись.
    void normalMapTiles() {
        lab::Pixels grey;
        grey.width = grey.height = 8;
        grey.pixels.assign(64, 0xFF606060u);
        auto const picture = brushOf(grey);
        auto const edges = brushOf(lab::readImage(L"BeveledEdges.jpg"));
        auto const sphere = brushOf(lab::readImage(L"SphericalWithMask.png"));

        sys::Effects::SceneLightingEffect light;
        light.AmbientAmount(0.0f);
        light.DiffuseAmount(0.5f);
        light.SpecularAmount(0.0f);
        light.NormalMapSource(sys::CompositionEffectSourceParameter {L"NormalMap"});

        effects::CompositeEffect sum;
        sum.Mode(canvas::CanvasComposite::Add);
        sum.Sources().Append(sys::CompositionEffectSourceParameter {L"ImageSource"});
        sum.Sources().Append(light);

        effects::CompositeEffect cut;
        cut.Mode(canvas::CanvasComposite::DestinationIn);
        cut.Sources().Append(sum);
        cut.Sources().Append(sys::CompositionEffectSourceParameter {L"NormalMap"});

        auto const factory = compositor.CreateEffectFactory(cut);
        line(u"SceneLightingEffect с NormalMapSource в графе образца: принято системной фабрикой.");

        auto const lit = compositor.CreateContainerVisual();
        lit.RelativeSizeAdjustment({1.0f, 1.0f});
        root.Children().InsertAtTop(lit);
        line(u"Системный, ContainerVisual.BorderMode без записи: " + nameOf(lit.BorderMode()));

        auto const wear = [&](sys::SpriteVisual const& sprite, sys::CompositionSurfaceBrush const& map) {
            auto const brush = factory.CreateBrush();
            brush.SetSourceParameter(L"ImageSource", picture);
            brush.SetSourceParameter(L"NormalMap", map);
            sprite.Brush(brush);
        };

        // Верхний ряд: граница не записана, записана мягкая, записана жёсткая; сфера с жёсткой.
        auto const untouched = tileAt(0, 0);
        line(u"Системный, SpriteVisual.BorderMode без записи: " + nameOf(untouched.BorderMode()));
        wear(untouched, edges);
        lit.Children().InsertAtTop(untouched);

        auto const soft = tileAt(1, 0);
        soft.BorderMode(sys::CompositionBorderMode::Soft);
        line(u"Системный, SpriteVisual.BorderMode после записи Soft: " + nameOf(soft.BorderMode()));
        wear(soft, edges);
        lit.Children().InsertAtTop(soft);

        auto const hard = tileAt(2, 0);
        hard.BorderMode(sys::CompositionBorderMode::Hard);
        line(u"Системный, SpriteVisual.BorderMode после записи Hard: " + nameOf(hard.BorderMode()));
        wear(hard, edges);
        lit.Children().InsertAtTop(hard);

        auto const ball = tileAt(3, 0);
        ball.BorderMode(sys::CompositionBorderMode::Hard);
        wear(ball, sphere);
        lit.Children().InsertAtTop(ball);

        // Справа во втором ряду: граница спрайта не записана, жёсткая записана
        // его родителю-контейнеру.
        auto const shell = compositor.CreateContainerVisual();
        shell.RelativeSizeAdjustment({1.0f, 1.0f});
        shell.BorderMode(sys::CompositionBorderMode::Hard);
        auto const inherited = tileAt(3, 1);
        wear(inherited, edges);
        shell.Children().InsertAtTop(inherited);
        lit.Children().InsertAtTop(shell);
        line(u"Системный, спрайт в контейнере с Hard, его BorderMode без записи: " + nameOf(inherited.BorderMode()));

        lamp = compositor.CreateDistantLight();
        line(u"Системный, DistantLight.Direction до записи: " + vector(lamp.Direction()));
        lamp.CoordinateSpace(lit);
        lamp.Targets().Add(lit);
        lamp.Direction({-0.8660254f, 0.0f, -0.5f});
        line(u"Системный, DistantLight.Direction сразу после записи (-0.866, 0, -0.5): " + vector(lamp.Direction()));

        // То же чтение у отделённого композитора: его объекты здесь не рисуются.
        auto const own = winrt::Microsoft::UI::Xaml::Media::CompositionTarget::GetCompositorForCurrentThread();
        auto const ownSprite = own.CreateSpriteVisual();
        line(u"Отделённый, SpriteVisual.BorderMode без записи: " + nameOf(ownSprite.BorderMode()));
        ownSprite.BorderMode(lifted::CompositionBorderMode::Hard);
        line(u"Отделённый, SpriteVisual.BorderMode после записи Hard: " + nameOf(ownSprite.BorderMode()));
        auto const ownLamp = own.CreateDistantLight();
        line(u"Отделённый, DistantLight.Direction до записи: " + vector(ownLamp.Direction()));
        ownLamp.Direction({-0.8660254f, 0.0f, -0.5f});
        line(u"Отделённый, DistantLight.Direction сразу после записи (-0.866, 0, -0.5): " +
             vector(ownLamp.Direction()));

        line(u"Верхний ряд окна: кромки без записи границы, с записанной мягкой, с жёсткой; сфера с жёсткой. "
             u"Справа во втором ряду: кромки, жёсткая граница у родителя. Свет строго справа.");
    }

    template <class Mode>
    static std::u16string nameOf(Mode mode) {
        switch (static_cast<int>(mode)) {
        case 0: return u"Inherit";
        case 1: return u"Soft";
        case 2: return u"Hard";
        default: return u"?";
        }
    }

    static std::u16string number(float value) {
        std::u16string words;
        if (value < 0.0f) {
            words += u"-";
            value = -value;
        }
        auto const thousandths = static_cast<std::uint32_t>(std::lround(value * 1000.0f));
        core::append_number(words, thousandths / 1000, 10);
        words += u".";
        std::uint32_t const rest = thousandths % 1000;
        if (rest < 100) words += u"0";
        if (rest < 10) words += u"0";
        core::append_number(words, rest, 10);
        return words;
    }

    static std::u16string vector(winrt::Windows::Foundation::Numerics::float3 const& value) {
        return u"(" + number(value.x) + u", " + number(value.y) + u", " + number(value.z) + u")";
    }

    // Четыре вопроса к фабрике на каждый эффект. Отвечает и системная, и
    // отделённая: у каждой свой класс входа-параметра. Возвращает, принят ли
    // эффект без входа.
    template <class Compositor, class Parameter>
    bool ask(Compositor const& factoryOwner, char16_t const* owner, Recipe const& recipe) {
        std::u16string const subject = std::u16string {owner} + u", " + recipe.name;
        bool bare = false;
        try {
            factoryOwner.CreateEffectFactory(recipe.make(Parameter {L"Height"}));
            line(subject + u", вход-параметр: принят");
        } catch (winrt::hresult_error const& error) {
            refused(subject + u", вход-параметр", error);
        }
        try {
            effects::ColorSourceEffect flat;
            flat.Color({255, 255, 255, 255});
            factoryOwner.CreateEffectFactory(recipe.make(flat));
            line(subject + u", вход-эффект ColorSourceEffect: принят");
        } catch (winrt::hresult_error const& error) {
            refused(subject + u", вход-эффект ColorSourceEffect", error);
        }
        try {
            factoryOwner.CreateEffectFactory(recipe.make(nullptr));
            line(subject + u", без входа: принят");
            bare = true;
        } catch (winrt::hresult_error const& error) {
            refused(subject + u", без входа", error);
        }
        try {
            factoryOwner.CreateEffectFactory(recipe.make(nullptr), {recipe.animated});
            line(subject + u", без входа, анимируемое свойство лампы: принято");
        } catch (winrt::hresult_error const& error) {
            refused(subject + u", без входа, анимируемое свойство лампы", error);
        }
        return bare;
    }

    void lightingEffects() {
        auto const own = winrt::Microsoft::UI::Xaml::Media::CompositionTarget::GetCompositorForCurrentThread();

        int index = 0;
        for (Recipe const& recipe : recipes()) {
            bool const bare =
                ask<sys::Compositor, sys::CompositionEffectSourceParameter>(compositor, u"Системный", recipe);
            ask<lifted::Compositor, lifted::CompositionEffectSourceParameter>(own, u"Отделённый", recipe);

            // Что эффект рисует без входа: плитка на системном композиторе.
            if (bare) {
                try {
                    auto const sprite = tileAt(index % 3, 1 + index / 3);
                    sprite.Brush(compositor.CreateEffectFactory(recipe.make(nullptr)).CreateBrush());
                    root.Children().InsertAtTop(sprite);
                } catch (winrt::hresult_error const& error) {
                    refused(std::u16string {u"Системный, "} + recipe.name + u", кисть на спрайте", error);
                }
            }
            ++index;
        }
        line(u"Второй и третий ряд окна: эффекты Win2D без входа, по три в ряд, в порядке списка.");
    }
};

}  // namespace

lab::Experiment lab::systemPage() {
    auto const model = std::make_shared<System>();

    auto stage = ScrollViewer {
        Padding {24},
        content = TextBlock {
            text = BindOutput {model->report},
            textWrapping = TextWrapping::Wrap,
            isTextSelectionEnabled = true,
        },
    };

    auto settings = lab::settingsPanel({
        lab::noteRow(u"Сцена этого опыта стоит в отдельном окне «wxl — проба системного композитора»: её рисует "
                     u"композитор самой Windows, Windows.UI.Composition. Слева -- ответы фабрик эффектов, системной "
                     u"и отделённой."),
    });

    return {stage, settings, model};
}
