// Проба XAML без дочернего окна.
//
// Окно -- как wxl::CompositionWindow: HWND с WS_EX_NOREDIRECTIONBITMAP, сцена --
// ContentIsland на DesktopAttachedSiteBridge, задний визуал получает размер
// синхронно в WM_SIZE. XAML вставлен одним из двух способов (ключ командной
// строки):
//
//   --dwxs  как сейчас: у сцены свой Microsoft.UI-композитор, XAML --
//           DesktopWindowXamlSource, дочернее окно, двигается MoveAndResize
//           из WM_SIZE;
//   --dwxs-shared  XAML по-старому, DesktopWindowXamlSource, но сцена -- на
//           композиторе потока XAML: один композитор без ChildSiteLink;
//   --link-own  связь, как --link, но у сцены свой композитор (как у
//           CompositionWindow сейчас и в тесте WinUI WindowlessXamlIslandTests);
//   --link-commit  то же, что --link, но после размера в WM_SIZE окно просит
//           композитор зафиксировать изменения (RequestCommitAsync);
//   --link  XAML -- XamlIsland, вставленный через ChildSiteLink в визуал-место
//           сцены; сцена построена на композиторе самого XamlIsland -- один
//           композитор на окно (запись wxl 0392); место и ActualSize связи --
//           из WM_SIZE.
//
// Вопросы: работает ли --link вообще (связь, отрисовка, щелчок, клавиша) и
// успевает ли задник при быстрой растяжке так же, как сейчас (запись 0285).
// Под окном лежит «шторка» -- окно чистого зелёного цвета: зелёный пиксель
// внутри клиентской области -- просвет, в который задник не успел. Растяжка
// программная (SetWindowPos по таймеру), кадр снимается с экрана перед
// следующим шагом.
//
// Проба прогоняет сценарий сама и закрывается; журнал --
// windowless-island-probe.log рядом с исполняемым файлом. Щелчок и клавиша --
// SendInput, только когда окно пробы впереди.

#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.h>

#include <windows.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

#include "launch.h"

namespace {

namespace mu = winrt::Microsoft::UI;
namespace muc = winrt::Microsoft::UI::Composition;
namespace content = winrt::Microsoft::UI::Content;
namespace xaml = winrt::Microsoft::UI::Xaml;
namespace controls = winrt::Microsoft::UI::Xaml::Controls;
using winrt::Windows::Foundation::Numerics::float2;

FILE* logFile = nullptr;
ULONGLONG startTick = 0;

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

constexpr COLORREF curtainColor = RGB(0, 255, 0);
constexpr winrt::Windows::UI::Color backdropColor{255, 200, 0, 200};  // задник -- пурпурный
constexpr int startWidth = 900;
constexpr int startHeight = 640;

struct Probe {
    bool link = false;
    bool requestCommit = false;  // --link-commit: после размера в WM_SIZE -- RequestCommitAsync
    bool sharedCompositor = false;  // --dwxs-shared: сцена на композиторе XAML, XAML -- DesktopWindowXamlSource
    bool ownSceneCompositor = false;  // --link-own: связь, но у сцены свой композитор
    HWND hwnd = nullptr;
    HWND curtain = nullptr;

    muc::Compositor compositor{nullptr};
    muc::SpriteVisual backdrop{nullptr};
    content::ContentIsland sceneIsland{nullptr};
    content::DesktopAttachedSiteBridge sceneBridge{nullptr};

    // --dwxs
    xaml::Hosting::DesktopWindowXamlSource xamlSource{nullptr};
    // --link
    xaml::XamlIsland xamlIsland{nullptr};
    muc::ContainerVisual placement{nullptr};
    content::ChildSiteLink siteLink{nullptr};

    controls::Grid page{nullptr};
    controls::Button button{nullptr};
    int clicks = 0;
    int keys = 0;

    int step = 0;
    int width = startWidth;
    int frames = 0;
    int framesWithGap = 0;
    int framesWithWhite = 0;
    long long maxGap = 0;
};

Probe* probe = nullptr;

// ---- снимок с экрана: сколько в клиентской области шторки и белого ----

struct Frame {
    long long green = 0;
    long long white = 0;
    long long backdrop = 0;
    long long total = 0;
};

Frame captureClient() {
    RECT client{};
    ::GetClientRect(probe->hwnd, &client);
    POINT origin{0, 0};
    ::ClientToScreen(probe->hwnd, &origin);
    int const w = client.right;
    int const h = client.bottom;
    Frame frame;
    if (w <= 0 || h <= 0) return frame;
    HDC const screen = ::GetDC(nullptr);
    HDC const memory = ::CreateCompatibleDC(screen);
    BITMAPINFO info{};
    info.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
    void* bits = nullptr;
    HBITMAP const bitmap = ::CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ const old = ::SelectObject(memory, bitmap);
    ::BitBlt(memory, 0, 0, w, h, screen, origin.x, origin.y, SRCCOPY | CAPTUREBLT);
    auto const* pixel = static_cast<std::uint32_t const*>(bits);
    for (long long i = 0, n = static_cast<long long>(w) * h; i < n; ++i) {
        std::uint32_t const bgr = pixel[i] & 0x00FFFFFF;  // BGRA в памяти -> 0x00RRGGBB
        if (bgr == 0x0000FF00) ++frame.green;
        else if (bgr == 0x00FFFFFF) ++frame.white;
        else if (bgr == 0x00C800C8) ++frame.backdrop;
    }
    frame.total = static_cast<long long>(w) * h;
    ::SelectObject(memory, old);
    ::DeleteObject(bitmap);
    ::DeleteDC(memory);
    ::ReleaseDC(nullptr, screen);
    return frame;
}

void noteFrame(char const* phase) {
    Frame const frame = captureClient();
    ++probe->frames;
    if (frame.green > 0) ++probe->framesWithGap;
    if (frame.white > 0) ++probe->framesWithWhite;
    if (frame.green > probe->maxGap) probe->maxGap = frame.green;
    say("  %s width %d: curtain %lld, white %lld, backdrop %lld of %lld", phase, probe->width, frame.green, frame.white,
        frame.backdrop, frame.total);
}

// ---- ввод ----

bool inFront() { return ::GetForegroundWindow() == probe->hwnd; }

void clickButton() {
    if (!inFront()) {
        say("  click skipped: probe window not in front");
        return;
    }
    auto const center = probe->button.TransformToVisual(nullptr).TransformPoint(
        {static_cast<float>(probe->button.ActualWidth() / 2), static_cast<float>(probe->button.ActualHeight() / 2)});
    double const scale = probe->button.XamlRoot().RasterizationScale();
    POINT point{static_cast<LONG>(center.X * scale), static_cast<LONG>(center.Y * scale)};
    ::ClientToScreen(probe->hwnd, &point);
    POINT saved{};
    ::GetCursorPos(&saved);
    ::SetCursorPos(point.x, point.y);
    INPUT inputs[2]{};
    inputs[0].type = inputs[1].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    ::SendInput(2, inputs, sizeof(INPUT));
    say("  click at %ld,%ld", point.x, point.y);
    ::SetCursorPos(saved.x, saved.y);
}

void key(WORD vk, bool down) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    ::SendInput(1, &input, sizeof(INPUT));
}

std::string focusedClass() {
    wchar_t name[128] = {};
    ::GetClassNameW(::GetFocus(), name, 128);
    return winrt::to_string(name);
}

// ---- сценарий по тикам в 16 мс ----

void resizeTo(int width) {
    probe->width = width;
    ::SetWindowPos(probe->hwnd, nullptr, 0, 0, width, startHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

void tick() {
    int const s = probe->step++;
    if (s == 60) {
        say("-- click on the XAML button (focus: %s)", focusedClass().c_str());
        clickButton();
    }
    if (s == 75) {
        say("-- Ctrl+plus (focus: %s, in front: %d)", focusedClass().c_str(), inFront() ? 1 : 0);
        if (inFront()) key(VK_CONTROL, true);
    }
    if (s == 77 && inFront()) key(VK_OEM_PLUS, true);
    if (s == 78 && inFront()) key(VK_OEM_PLUS, false);
    if (s == 80) key(VK_CONTROL, false);
    if (s == 95) {
        say("   clicks seen: %d, Ctrl+plus seen by XAML: %d", probe->clicks, probe->keys);
        say("-- grow: +30 px per 16 ms tick");
        noteFrame("before");
    }
    // Каждый тик: снимок кадра, успевшего с прошлого шага, затем новый шаг.
    if (s > 95 && s <= 115) {
        noteFrame("grow");
        resizeTo(probe->width + 30);
    }
    if (s == 125) {
        noteFrame("settled");
        say("-- shrink: -30 px per tick");
    }
    if (s > 125 && s <= 145) {
        noteFrame("shrink");
        resizeTo(probe->width - 30);
    }
    if (s == 155) {
        noteFrame("settled");
        say("-- grow fast: +80 px per tick");
    }
    if (s > 155 && s <= 165) {
        noteFrame("fast");
        resizeTo(probe->width + 80);
    }
    if (s == 175) {
        noteFrame("settled");
        say("== %s: frames %d, with curtain (gap) %d, max curtain pixels %lld, with white %d",
            probe->ownSceneCompositor ? "link-own (XamlIsland + ChildSiteLink, own scene compositor)"
            : probe->sharedCompositor ? "dwxs-shared (DesktopWindowXamlSource, scene on the XAML compositor)"
            : probe->requestCommit ? "link-commit (one compositor, RequestCommitAsync in WM_SIZE)"
            : probe->link        ? "link (XamlIsland + ChildSiteLink, one compositor)"
                                 : "dwxs (DesktopWindowXamlSource, own scene compositor)",
            probe->frames, probe->framesWithGap, probe->maxGap, probe->framesWithWhite);
        say("done");
        ::KillTimer(probe->hwnd, 1);
        ::PostMessageW(probe->hwnd, WM_CLOSE, 0, 0);
    }
}

// ---- окно, сцена, остров ----

controls::Grid buildPage() {
    controls::Grid page;
    page.Background(xaml::Media::SolidColorBrush{winrt::Windows::UI::Color{255, 40, 40, 60}});
    controls::StackPanel lines;
    probe->button = controls::Button{};
    probe->button.Content(winrt::box_value(L"Кнопка XAML"));
    probe->button.Margin(xaml::ThicknessHelper::FromUniformLength(12));
    probe->button.Click([](auto&&, auto&&) {
        ++probe->clicks;
        say("  button Click");
    });
    lines.Children().Append(probe->button);
    // Тяжёлая вёрстка: переносы строк пересчитываются на каждой ширине.
    for (int i = 1; i <= 300; ++i) {
        controls::TextBlock line;
        line.Text(winrt::hstring{L"Строка " + std::to_wstring(i) +
                                 L": текст, который переносится по ширине окна и заставляет XAML пересчитывать вёрстку"});
        line.TextWrapping(xaml::TextWrapping::Wrap);
        line.Foreground(xaml::Media::SolidColorBrush{winrt::Windows::UI::Color{255, 220, 220, 220}});
        lines.Children().Append(line);
    }
    page.Children().Append(lines);
    page.PreviewKeyDown([](auto&&, xaml::Input::KeyRoutedEventArgs const& args) {
        if (args.Key() == static_cast<winrt::Windows::System::VirtualKey>(VK_OEM_PLUS)) {
            ++probe->keys;
            say("  page PreviewKeyDown Ctrl+plus");
        }
    });
    return page;
}

void resize(int width, int height) {
    float2 const size{static_cast<float>(width), static_cast<float>(height)};
    if (probe->backdrop) probe->backdrop.Size(size);
    if (probe->xamlSource) probe->xamlSource.SiteBridge().MoveAndResize({0, 0, width, height});
    if (probe->placement) probe->placement.Size(size);
    if (probe->siteLink) probe->siteLink.ActualSize(size);
    if (probe->requestCommit && probe->compositor) probe->compositor.RequestCommitAsync();
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

mu::WindowId windowIdOf(HWND hwnd) { return mu::WindowId{static_cast<uint64_t>(reinterpret_cast<uintptr_t>(hwnd))}; }

void buildDwxs() {
    // --dwxs-shared: сцена на композиторе потока XAML -- один композитор, XAML по-старому.
    probe->compositor = probe->sharedCompositor
                            ? xaml::Hosting::ElementCompositionPreview::GetElementVisual(probe->page).Compositor()
                            : muc::Compositor{};
    probe->backdrop = probe->compositor.CreateSpriteVisual();
    probe->backdrop.Brush(probe->compositor.CreateColorBrush(backdropColor));
    probe->sceneIsland = content::ContentIsland::Create(probe->backdrop);
    probe->sceneBridge = content::DesktopAttachedSiteBridge::CreateFromWindowId(
        mu::Dispatching::DispatcherQueue::GetForCurrentThread(), windowIdOf(probe->hwnd));
    probe->sceneBridge.Connect(probe->sceneIsland);

    probe->xamlSource = xaml::Hosting::DesktopWindowXamlSource{};
    probe->xamlSource.Initialize(windowIdOf(probe->hwnd));
    probe->xamlSource.Content(probe->page);
    probe->xamlSource.SiteBridge().Show();
    say("dwxs: scene compositor differs from the XAML compositor: %d",
        probe->compositor != xaml::Hosting::ElementCompositionPreview::GetElementVisual(probe->page).Compositor() ? 1 : 0);
}

void buildLink() {
    probe->xamlIsland = xaml::XamlIsland{};
    probe->xamlIsland.Content(probe->page);
    // Один композитор: сцена -- на композиторе потока XAML, том же, на котором
    // рисует остров (ContentIsland своего композитора не отдаёт).
    // --link-own: сцене -- свой композитор, как у CompositionWindow и в тесте WinUI
    // WindowlessXamlIslandTests; остров XAML -- на своём, через ChildSiteLink.
    probe->compositor = probe->ownSceneCompositor
                            ? muc::Compositor{}
                            : xaml::Hosting::ElementCompositionPreview::GetElementVisual(probe->page).Compositor();
    probe->backdrop = probe->compositor.CreateSpriteVisual();
    probe->backdrop.Brush(probe->compositor.CreateColorBrush(backdropColor));
    probe->placement = probe->compositor.CreateContainerVisual();
    probe->backdrop.Children().InsertAtTop(probe->placement);
    probe->sceneIsland = content::ContentIsland::Create(probe->backdrop);
    probe->sceneBridge = content::DesktopAttachedSiteBridge::CreateFromWindowId(
        mu::Dispatching::DispatcherQueue::GetForCurrentThread(), windowIdOf(probe->hwnd));
    probe->sceneBridge.Connect(probe->sceneIsland);
    try {
        probe->siteLink = content::ChildSiteLink::Create(probe->sceneIsland, probe->placement);
        probe->siteLink.Connect(probe->xamlIsland.ContentIsland());
        say("link: ChildSiteLink created and connected");
    } catch (winrt::hresult_error const& error) {
        say("link: ChildSiteLink FAILED %08x %s", static_cast<unsigned>(error.code().value),
            winrt::to_string(error.message()).c_str());
    }
}

}  // namespace

wxl::Teardown wxl_launched() {
    wchar_t path[MAX_PATH] = {};
    ::GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring logPath{path};
    logPath.resize(logPath.find_last_of(L'\\') + 1);
    logPath += L"windowless-island-probe.log";
    logFile = ::_wfopen(logPath.c_str(), L"w");
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
    probe->link = std::wstring_view{::GetCommandLineW()}.find(L"--link") != std::wstring_view::npos;
    probe->requestCommit = std::wstring_view{::GetCommandLineW()}.find(L"--link-commit") != std::wstring_view::npos;
    probe->sharedCompositor = std::wstring_view{::GetCommandLineW()}.find(L"--dwxs-shared") != std::wstring_view::npos;
    probe->ownSceneCompositor = std::wstring_view{::GetCommandLineW()}.find(L"--link-own") != std::wstring_view::npos;
    say("mode: %s", probe->ownSceneCompositor ? "--link-own" : probe->requestCommit ? "--link-commit" : probe->link ? "--link" : probe->sharedCompositor ? "--dwxs-shared" : "--dwxs");

    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = ::GetModuleHandleW(nullptr);
    wc.lpszClassName = L"wxl.WindowlessIslandProbe";
    wc.hCursor = ::LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    ::RegisterClassExW(&wc);

    // Шторка: всё, что проглядывает сквозь окно пробы, -- чистый зелёный.
    WNDCLASSEXW curtainClass{sizeof(WNDCLASSEXW)};
    curtainClass.lpfnWndProc = ::DefWindowProcW;
    curtainClass.hInstance = wc.hInstance;
    curtainClass.lpszClassName = L"wxl.WindowlessIslandProbe.Curtain";
    curtainClass.hbrBackground = ::CreateSolidBrush(curtainColor);
    ::RegisterClassExW(&curtainClass);
    probe->curtain = ::CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, curtainClass.lpszClassName, L"", WS_POPUP,
                                       100, 60, 1700, 900, nullptr, nullptr, wc.hInstance, nullptr);

    probe->hwnd = ::CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP, wc.lpszClassName, L"Проба XAML без дочернего окна",
                                    WS_OVERLAPPEDWINDOW, 150, 100, startWidth, startHeight, nullptr, nullptr,
                                    wc.hInstance, nullptr);

    probe->page = buildPage();
    if (probe->link) {
        buildLink();
    } else {
        buildDwxs();
    }
    RECT client{};
    ::GetClientRect(probe->hwnd, &client);
    resize(client.right, client.bottom);

    ::ShowWindow(probe->curtain, SW_SHOWNOACTIVATE);
    ::ShowWindow(probe->hwnd, SW_SHOW);
    ::SetWindowPos(probe->curtain, probe->hwnd, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    ::SetForegroundWindow(probe->hwnd);
    ::SetTimer(probe->hwnd, 1, 16, nullptr);
    say("shown");

    return [](wxl::TeardownReason) {
        if (probe->siteLink) probe->siteLink.Close();
        if (probe->xamlIsland) probe->xamlIsland.Close();
        if (probe->curtain) ::DestroyWindow(probe->curtain);
        say("teardown");
        if (logFile) std::fclose(logFile);
    };
}
