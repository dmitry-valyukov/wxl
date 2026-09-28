// Проба стекла в острове XAML: видит ли backdrop-кисть острова сцену окна.
//
// GlassEffect (запись wxl 0396) держит два стекла: на сцене окна и в острове
// XAML, потому что backdrop-кисть в острове не видит сцену -- остров стоит
// дочерним окном DesktopWindowXamlSource. Вопрос: если XAML вставить
// XamlIsland через ChildSiteLink в сцену (одно дерево), увидит ли кисть в
// острове и сцену, и XAML -- так что хватит одного стекла?
//
// Окно -- как wxl::CompositionWindow (WS_EX_NOREDIRECTIONBITMAP, сцена --
// ContentIsland на DesktopAttachedSiteBridge). На сцене -- вертикальные полосы
// чистого красного и синего. Остров XAML прозрачный; в нём -- прямоугольник
// чистого зелёного и над частью полос и частью прямоугольника -- «стекло»:
// визуал элемента (SetElementChildVisual) с кистью GaussianBlur поверх
// CreateBackdropBrush, как у GlassEffect. Если кисть видит то, что под ней,
// под стеклом чистых цветов почти нет -- они смешаны размытием; если не видит,
// её вывод прозрачен и сквозь стекло видны чистые полосы и зелёный.
//
// Режимы (ключ командной строки):
//   --dwxs      как сейчас: свой композитор сцены, DesktopWindowXamlSource;
//   --link      XamlIsland через ChildSiteLink, сцена на композиторе XAML;
//   --link-own  XamlIsland через ChildSiteLink, у сцены свой композитор.
//
// Проба снимает клиентскую область с экрана, считает пиксели под стеклом и
// закрывается; журнал -- glass-probe.log, снимок -- glass-probe-<режим>.bmp
// рядом с исполняемым файлом. Окно на время снимка поверх остальных.

#include <winrt/Microsoft.Graphics.Canvas.Effects.h>
#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Effects.h>
#include <winrt/Windows.UI.h>

#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "launch.h"

namespace {

namespace mu = winrt::Microsoft::UI;
namespace muc = winrt::Microsoft::UI::Composition;
namespace content = winrt::Microsoft::UI::Content;
namespace xaml = winrt::Microsoft::UI::Xaml;
namespace controls = winrt::Microsoft::UI::Xaml::Controls;
namespace effects = winrt::Microsoft::Graphics::Canvas::Effects;
using winrt::Windows::Foundation::Numerics::float2;
using winrt::Windows::Foundation::Numerics::float3;
using winrt::Windows::UI::Color;

FILE* logFile = nullptr;
ULONGLONG startTick = 0;
std::wstring folder;

void say(const char* format, ...) {
    if (!logFile) return;
    std::fprintf(logFile, "%7llu ", ::GetTickCount64() - startTick);
    va_list args;
    va_start(args, format);
    std::vfprintf(logFile, format, args);
    va_end(args);
    std::fputc('\n', logFile);
    std::fflush(logFile);
}

enum class Mode { Dwxs, Link, LinkOwn };

struct Probe {
    Mode mode = Mode::Dwxs;
    HWND hwnd = nullptr;

    muc::Compositor sceneCompositor{nullptr};
    muc::SpriteVisual backdrop{nullptr};
    content::ContentIsland sceneIsland{nullptr};
    content::DesktopAttachedSiteBridge sceneBridge{nullptr};

    xaml::Hosting::DesktopWindowXamlSource xamlSource{nullptr};
    xaml::XamlIsland xamlIsland{nullptr};
    muc::ContainerVisual placement{nullptr};
    content::ChildSiteLink siteLink{nullptr};

    controls::Grid page{nullptr};
    controls::Border pane{nullptr};
    xaml::Shapes::Rectangle green{nullptr};
    muc::SpriteVisual glass{nullptr};

    int step = 0;
};

Probe* probe = nullptr;

char const* modeName() {
    switch (probe->mode) {
        case Mode::Dwxs: return "dwxs";
        case Mode::Link: return "link";
        case Mode::LinkOwn: return "link-own";
    }
    return "?";
}

// ---- сцена: полосы чистого красного и синего ----

void addStripes() {
    auto const compositor = probe->sceneCompositor;
    for (int i = 0; i < 40; ++i) {
        auto stripe = compositor.CreateSpriteVisual();
        stripe.Brush(compositor.CreateColorBrush(i % 2 == 0 ? Color{255, 255, 0, 0} : Color{255, 0, 0, 255}));
        stripe.Offset(float3{static_cast<float>(40 + i * 16), 30.0f, 0.0f});
        stripe.Size(float2{16.0f, 520.0f});
        probe->backdrop.Children().InsertAtBottom(stripe);
    }
}

void buildScene(muc::Compositor const& compositor) {
    probe->sceneCompositor = compositor;
    probe->backdrop = compositor.CreateSpriteVisual();
    probe->backdrop.Brush(compositor.CreateColorBrush(Color{255, 32, 32, 32}));
    addStripes();
    probe->sceneIsland = content::ContentIsland::Create(probe->backdrop);
    probe->sceneBridge = content::DesktopAttachedSiteBridge::CreateFromWindowId(
        mu::Dispatching::DispatcherQueue::GetForCurrentThread(),
        mu::WindowId{static_cast<uint64_t>(reinterpret_cast<uintptr_t>(probe->hwnd))});
    probe->sceneBridge.Connect(probe->sceneIsland);
}

// ---- остров: прозрачная страница, зелёный прямоугольник и стекло ----

controls::Grid buildPage() {
    controls::Grid page;  // без фона -- прозрачна, сквозь неё видна сцена
    probe->green = xaml::Shapes::Rectangle{};
    probe->green.Fill(xaml::Media::SolidColorBrush{Color{255, 0, 255, 0}});
    probe->green.Width(260);
    probe->green.Height(90);
    probe->green.HorizontalAlignment(xaml::HorizontalAlignment::Left);
    probe->green.VerticalAlignment(xaml::VerticalAlignment::Top);
    probe->green.Margin(xaml::Thickness{120, 190, 0, 0});
    page.Children().Append(probe->green);

    // Стекло над частью полос и частью зелёного.
    probe->pane = controls::Border{};
    probe->pane.Width(240);
    probe->pane.Height(170);
    probe->pane.HorizontalAlignment(xaml::HorizontalAlignment::Left);
    probe->pane.VerticalAlignment(xaml::VerticalAlignment::Top);
    probe->pane.Margin(xaml::Thickness{200, 120, 0, 0});
    page.Children().Append(probe->pane);
    return page;
}

void attachGlass() {
    auto const compositor = xaml::Hosting::ElementCompositionPreview::GetElementVisual(probe->pane).Compositor();
    effects::GaussianBlurEffect blur;
    blur.BlurAmount(12.0f);
    blur.BorderMode(effects::EffectBorderMode::Hard);
    blur.Source(muc::CompositionEffectSourceParameter{L"backdrop"});
    auto const brush = compositor.CreateEffectFactory(blur).CreateBrush();
    brush.SetSourceParameter(L"backdrop", compositor.CreateBackdropBrush());
    probe->glass = compositor.CreateSpriteVisual();
    probe->glass.Brush(brush);
    probe->glass.Size(float2{static_cast<float>(probe->pane.ActualWidth()), static_cast<float>(probe->pane.ActualHeight())});
    xaml::Hosting::ElementCompositionPreview::SetElementChildVisual(probe->pane, probe->glass);
    say("glass attached: %.0f x %.0f DIP, glass compositor == scene compositor: %d", probe->pane.ActualWidth(),
        probe->pane.ActualHeight(), compositor == probe->sceneCompositor ? 1 : 0);
}

// ---- снимок и подсчёт ----

struct Box {
    int left, top, right, bottom;
};

Box physicalBox(xaml::FrameworkElement const& element) {
    auto const origin = element.TransformToVisual(nullptr).TransformPoint({0, 0});
    double const scale = element.XamlRoot().RasterizationScale();
    return {static_cast<int>(origin.X * scale), static_cast<int>(origin.Y * scale),
            static_cast<int>((origin.X + element.ActualWidth()) * scale),
            static_cast<int>((origin.Y + element.ActualHeight()) * scale)};
}

struct Counts {
    long long pureRed = 0, pureBlue = 0, pureGreen = 0, mixed = 0, other = 0, total = 0;
};

void count(std::vector<std::uint32_t> const& pixels, int width, Box box, Counts& c) {
    // Край стекла (размытие с жёсткой границей) не считаем -- 12 пикселей внутрь.
    for (int y = box.top + 12; y < box.bottom - 12; ++y) {
        for (int x = box.left + 12; x < box.right - 12; ++x) {
            std::uint32_t const p = pixels[static_cast<size_t>(y) * width + x];
            int const r = (p >> 16) & 0xFF, g = (p >> 8) & 0xFF, b = p & 0xFF;
            ++c.total;
            if (r > 245 && g < 10 && b < 10) ++c.pureRed;
            else if (b > 245 && r < 10 && g < 10) ++c.pureBlue;
            else if (g > 245 && r < 10 && b < 10) ++c.pureGreen;
            else if ((r > 40 && b > 40) || (g > 40 && (r > 40 || b > 40))) ++c.mixed;
            else ++c.other;
        }
    }
}

void report(char const* what, Counts const& c) {
    say("  %s: pure red %lld, pure blue %lld, pure green %lld, mixed %lld, other %lld of %lld", what, c.pureRed,
        c.pureBlue, c.pureGreen, c.mixed, c.other, c.total);
}

void saveBmp(std::vector<std::uint32_t> const& pixels, int w, int h) {
    std::wstring const path = folder + L"glass-probe-" + std::wstring{winrt::to_hstring(modeName())} + L".bmp";
    FILE* file = ::_wfopen(path.c_str(), L"wb");
    if (!file) return;
    BITMAPFILEHEADER fh{};
    BITMAPINFOHEADER ih{sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + static_cast<DWORD>(pixels.size() * 4);
    std::fwrite(&fh, sizeof(fh), 1, file);
    std::fwrite(&ih, sizeof(ih), 1, file);
    std::fwrite(pixels.data(), 4, pixels.size(), file);
    std::fclose(file);
}

void measure() {
    RECT client{};
    ::GetClientRect(probe->hwnd, &client);
    POINT origin{0, 0};
    ::ClientToScreen(probe->hwnd, &origin);
    int const w = client.right, h = client.bottom;
    std::vector<std::uint32_t> pixels(static_cast<size_t>(w) * h);
    HDC const screen = ::GetDC(nullptr);
    HDC const memory = ::CreateCompatibleDC(screen);
    HBITMAP const bitmap = ::CreateCompatibleBitmap(screen, w, h);
    HGDIOBJ const old = ::SelectObject(memory, bitmap);
    ::BitBlt(memory, 0, 0, w, h, screen, origin.x, origin.y, SRCCOPY | CAPTUREBLT);
    BITMAPINFOHEADER ih{sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
    ::GetDIBits(memory, bitmap, 0, h, pixels.data(), reinterpret_cast<BITMAPINFO*>(&ih), DIB_RGB_COLORS);
    ::SelectObject(memory, old);
    ::DeleteObject(bitmap);
    ::DeleteDC(memory);
    ::ReleaseDC(nullptr, screen);
    saveBmp(pixels, w, h);

    Box const pane = physicalBox(probe->pane);
    Box const green = physicalBox(probe->green);
    say("pane %d,%d-%d,%d; green %d,%d-%d,%d (physical)", pane.left, pane.top, pane.right, pane.bottom, green.left,
        green.top, green.right, green.bottom);

    // Сцена под стеклом: верхняя часть стекла, дальше радиуса размытия от
    // зелёного. Видит кисть сцену -- полосы смешаны в пурпур, не видит --
    // сквозь прозрачный вывод видны чистые полосы.
    Counts overScene;
    count(pixels, w, {pane.left, pane.top, pane.right, green.top - 30}, overScene);
    report("under glass, over the scene stripes", overScene);
    // XAML под стеклом: полоса стекла правее зелёного, в радиусе размытия.
    // Видит кисть XAML -- зелёный расплывается на неё (смешанные пиксели).
    Counts nearXaml;
    count(pixels, w, {green.right - 12, green.top, std::min(pane.right, green.right + 40), green.bottom}, nearXaml);
    report("under glass, right of the XAML green edge", nearXaml);
    // Контроль: тот же край зелёного вне стекла -- левый край, левее стекла.
    Counts control;
    count(pixels, w, {green.left - 28, green.top, green.left + 12, green.bottom}, control);
    report("control: the green edge without glass", control);

    bool const sceneBlurred = overScene.mixed * 2 > overScene.total;
    bool const xamlBlurred = nearXaml.mixed > control.mixed + nearXaml.total / 10;
    say("== %s: scene under glass %s; XAML under glass %s", modeName(),
        sceneBlurred ? "BLURRED -- the brush sees the scene" : "sharp -- the brush does not see the scene",
        xamlBlurred ? "BLURRED -- the brush sees XAML" : "sharp -- the brush does not see XAML");
}

// ---- окно ----

void resize(int width, int height) {
    float2 const size{static_cast<float>(width), static_cast<float>(height)};
    if (probe->backdrop) probe->backdrop.Size(size);
    if (probe->xamlSource) probe->xamlSource.SiteBridge().MoveAndResize({0, 0, width, height});
    if (probe->placement) probe->placement.Size(size);
    if (probe->siteLink) probe->siteLink.ActualSize(size);
}

void tick() {
    int const s = probe->step++;
    if (s == 40) attachGlass();
    if (s == 110) {
        measure();
        say("done");
        ::KillTimer(probe->hwnd, 1);
        ::PostMessageW(probe->hwnd, WM_CLOSE, 0, 0);
    }
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (!probe || hwnd != probe->hwnd) return ::DefWindowProcW(hwnd, message, wparam, lparam);
    switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE:
            if (wparam != SIZE_MINIMIZED) resize(LOWORD(lparam), HIWORD(lparam));
            break;
        case WM_TIMER:
            tick();
            return 0;
        case WM_CLOSE:
            ::DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            xaml::Application::Current().Exit();
            return 0;
    }
    return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

wxl::Teardown wxl_launched() {
    wchar_t path[MAX_PATH] = {};
    ::GetModuleFileNameW(nullptr, path, MAX_PATH);
    folder = path;
    folder.resize(folder.find_last_of(L'\\') + 1);
    logFile = ::_wfopen((folder + L"glass-probe.log").c_str(), L"w");
    startTick = ::GetTickCount64();

    // --system-engine: движок композиции ОС вместо движка в процессе (Windows App
    // SDK 2.3, Limited Access Feature); ставится до первого объекта композиции.
    if (std::wstring_view{::GetCommandLineW()}.find(L"--system-engine") != std::wstring_view::npos) {
        bool set = false;
        try {
            set = muc::CompositionEngine::TrySetProcessEngine(muc::CompositionEngineType::System);
            say("CompositionEngine::TrySetProcessEngine(System): %d", set ? 1 : 0);
        } catch (winrt::hresult_error const& error) {
            say("CompositionEngine::TrySetProcessEngine(System) threw %08x %s", static_cast<unsigned>(error.code().value),
                winrt::to_string(error.message()).c_str());
        }
    }

    probe = new Probe{};
    std::wstring_view const commandLine{::GetCommandLineW()};
    probe->mode = commandLine.find(L"--link-own") != std::wstring_view::npos ? Mode::LinkOwn
                  : commandLine.find(L"--link") != std::wstring_view::npos ? Mode::Link
                                                                           : Mode::Dwxs;
    say("mode: %s", modeName());

    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = ::GetModuleHandleW(nullptr);
    wc.lpszClassName = L"wxl.GlassProbe";
    wc.hCursor = ::LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    ::RegisterClassExW(&wc);
    probe->hwnd = ::CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP | WS_EX_TOPMOST, wc.lpszClassName, L"Проба стекла",
                                    WS_OVERLAPPEDWINDOW, 150, 100, 900, 640, nullptr, nullptr, wc.hInstance, nullptr);

    probe->page = buildPage();
    if (probe->mode == Mode::Dwxs) {
        buildScene(muc::Compositor{});
        probe->xamlSource = xaml::Hosting::DesktopWindowXamlSource{};
        probe->xamlSource.Initialize(mu::WindowId{static_cast<uint64_t>(reinterpret_cast<uintptr_t>(probe->hwnd))});
        probe->xamlSource.Content(probe->page);
        probe->xamlSource.SiteBridge().Show();
    } else {
        probe->xamlIsland = xaml::XamlIsland{};
        probe->xamlIsland.Content(probe->page);
        buildScene(probe->mode == Mode::Link
                       ? xaml::Hosting::ElementCompositionPreview::GetElementVisual(probe->page).Compositor()
                       : muc::Compositor{});
        probe->placement = probe->sceneCompositor.CreateContainerVisual();
        probe->backdrop.Children().InsertAtTop(probe->placement);
        try {
            probe->siteLink = content::ChildSiteLink::Create(probe->sceneIsland, probe->placement);
            probe->siteLink.Connect(probe->xamlIsland.ContentIsland());
            say("ChildSiteLink connected");
        } catch (winrt::hresult_error const& error) {
            say("ChildSiteLink FAILED %08x", static_cast<unsigned>(error.code().value));
        }
    }
    RECT client{};
    ::GetClientRect(probe->hwnd, &client);
    resize(client.right, client.bottom);

    ::ShowWindow(probe->hwnd, SW_SHOW);
    ::SetTimer(probe->hwnd, 1, 16, nullptr);
    say("shown");

    return [](wxl::TeardownReason) {
        if (probe->siteLink) probe->siteLink.Close();
        if (probe->xamlIsland) probe->xamlIsland.Close();
        say("teardown");
        if (logFile) std::fclose(logFile);
    };
}
