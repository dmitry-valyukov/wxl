// Проба: кнопка у правого края острова при смене масштаба острова.
//
// В Беседке кнопки окна (правый край строки заголовка) при смене масштаба
// уезжают от угла. Кнопки стоят в корневом Grid острова; с коммита 7936f51
// (жест масштабирования) над корнем стоит ScrollViewer-датчик. Проба меняет
// масштаб острова так же, как ZoomEffect (OverrideScale моста: DPI × масштаб;
// пределы датчика -- 0,5 / масштаб и 3 / масштаб) и после каждой смены меряет,
// где правый край кнопки относительно правого края окна, какой ZoomFactor у
// датчика и какой ширины корень против ширины XamlRoot.
//
//   --sensor  над корнем ScrollViewer-датчик, как в CompositionWindow;
//   --plain   корень -- сам содержимое острова, как до 7936f51.
//
// Ввода нет; журнал -- sensor-scale-probe.log рядом с исполняемым файлом.

#include "platform.h"

#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <string_view>

#include "launch.h"

namespace {

namespace mu = winrt::Microsoft::UI;
namespace xaml = winrt::Microsoft::UI::Xaml;
namespace controls = winrt::Microsoft::UI::Xaml::Controls;

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

constexpr double zooms[] = {1.0, 0.9, 0.75, 2.0 / 3.0, 0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 1.0, 0.8, 1.0};

struct Probe {
    bool sensorMode = false;
    HWND hwnd = nullptr;
    xaml::Hosting::DesktopWindowXamlSource source{nullptr};
    controls::ScrollViewer sensor{nullptr};
    controls::Grid root{nullptr};
    controls::Button button{nullptr};
    double zoom = 1.0;
    int step = 0;
    int mismatches = 0;
};

Probe* probe = nullptr;

// Как applyZoom в CompositionWindow: масштаб моста -- масштаб окна × увеличение.
void applyZoom(double zoom) {
    probe->zoom = zoom;
    auto const bridge = probe->source.SiteBridge();
    float const parent = bridge.SiteView().ParentScale();
    float const wanted = parent * static_cast<float>(zoom);
    bridge.OverrideScale(wanted);
    float const got = bridge.SiteView().RasterizationScale();
    if (std::abs(got - wanted) > 0.001f && std::abs(got - wanted * parent) <= 0.001f) {
        bridge.OverrideScale(static_cast<float>(zoom));
    }
    if (probe->sensor) {
        probe->sensor.MinZoomFactor(static_cast<float>(0.5 / zoom));
        probe->sensor.MaxZoomFactor(static_cast<float>(3.0 / zoom));
    }
}

void measure() {
    RECT client{};
    ::GetClientRect(probe->hwnd, &client);
    auto const xamlRoot = probe->root.XamlRoot();
    double const scale = xamlRoot.RasterizationScale();
    auto const topLeft = probe->button.TransformToVisual(nullptr).TransformPoint({0, 0});
    double const right = (topLeft.X + probe->button.ActualWidth()) * scale;
    double const gap = client.right - right;
    bool const off = std::abs(gap - 8.0 * scale) > 3.0;  // кнопка стоит с полем 8 DIP
    if (off) ++probe->mismatches;
    say("zoom %.3f: raster %.3f; window %ld px; button right %.1f px, gap %.1f px (expected %.1f)%s; "
        "XamlRoot width %.1f DIP, root width %.1f DIP; sensor zoom %s",
        probe->zoom, scale, client.right, right, gap, 8.0 * scale, off ? " <-- OFF THE EDGE" : "",
        xamlRoot.Size().Width, probe->root.ActualWidth(),
        probe->sensor ? std::to_string(probe->sensor.ZoomFactor()).c_str() : "-");
}

void tick() {
    int const s = probe->step++;
    int const phase = s / 40;
    int const offset = s % 40;
    constexpr int count = static_cast<int>(sizeof(zooms) / sizeof(zooms[0]));
    if (phase >= count) {
        say("== %s: %d of %d measurements off the edge", probe->sensorMode ? "sensor" : "plain", probe->mismatches,
            count);
        say("done");
        ::KillTimer(probe->hwnd, 1);
        ::PostMessageW(probe->hwnd, WM_CLOSE, 0, 0);
        return;
    }
    if (offset == 0) applyZoom(zooms[phase]);
    if (offset == 35) measure();
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (!probe || hwnd != probe->hwnd) return ::DefWindowProcW(hwnd, message, wparam, lparam);
    switch (message) {
        case WM_SIZE:
            if (probe->source) {
                RECT client{};
                ::GetClientRect(hwnd, &client);
                probe->source.SiteBridge().MoveAndResize({0, 0, client.right, client.bottom});
            }
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
    std::wstring logPath{path};
    logPath.resize(logPath.find_last_of(L'\\') + 1);
    logPath += L"sensor-scale-probe.log";
    logFile = ::_wfopen(logPath.c_str(), L"w");
    startTick = ::GetTickCount64();

    probe = new Probe{};
    probe->sensorMode = std::wstring_view{::GetCommandLineW()}.find(L"--sensor") != std::wstring_view::npos;
    say("mode: %s", probe->sensorMode ? "sensor" : "plain");

    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = ::GetModuleHandleW(nullptr);
    wc.lpszClassName = L"wxl.SensorScaleProbe";
    wc.hCursor = ::LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    ::RegisterClassExW(&wc);
    probe->hwnd = ::CreateWindowExW(0, wc.lpszClassName, L"Проба датчика и масштаба", WS_OVERLAPPEDWINDOW, 200, 120,
                                    900, 600, nullptr, nullptr, wc.hInstance, nullptr);

    // Корень -- как в CompositionWindow: Grid в две строки, в верхней кнопка у
    // правого края (как кнопки окна), нижняя -- содержимое.
    probe->root = controls::Grid{};
    for (auto const height : {xaml::GridLengthHelper::Auto(),
                              xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star)}) {
        controls::RowDefinition row;
        row.Height(height);
        probe->root.RowDefinitions().Append(row);
    }
    probe->button = controls::Button{};
    probe->button.Content(winrt::box_value(L"✕"));
    probe->button.HorizontalAlignment(xaml::HorizontalAlignment::Right);
    probe->button.Margin(xaml::Thickness{0, 4, 8, 4});
    probe->root.Children().Append(probe->button);

    probe->source = xaml::Hosting::DesktopWindowXamlSource{};
    probe->source.Initialize(mu::WindowId{static_cast<uint64_t>(reinterpret_cast<uintptr_t>(probe->hwnd))});
    if (probe->sensorMode) {
        // Датчик -- как ensureZoomSensor в CompositionWindow.
        probe->sensor = controls::ScrollViewer{};
        probe->sensor.ZoomMode(controls::ZoomMode::Enabled);
        probe->sensor.IsZoomInertiaEnabled(false);
        probe->sensor.HorizontalScrollMode(controls::ScrollMode::Disabled);
        probe->sensor.VerticalScrollMode(controls::ScrollMode::Disabled);
        probe->sensor.HorizontalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
        probe->sensor.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
        probe->sensor.ViewChanged([](auto&&, controls::ScrollViewerViewChangedEventArgs const& args) {
            say("  sensor ViewChanged zoom=%.4f intermediate=%d", probe->sensor.ZoomFactor(),
                args.IsIntermediate() ? 1 : 0);
        });
        probe->source.Content(probe->sensor);
        probe->sensor.Content(probe->root);
    } else {
        probe->source.Content(probe->root);
    }
    RECT client{};
    ::GetClientRect(probe->hwnd, &client);
    probe->source.SiteBridge().MoveAndResize({0, 0, client.right, client.bottom});
    probe->source.SiteBridge().Show();
    applyZoom(1.0);

    ::ShowWindow(probe->hwnd, SW_SHOWNOACTIVATE);
    ::SetTimer(probe->hwnd, 1, 16, nullptr);
    say("shown");

    return [](wxl::TeardownReason) {
        say("teardown");
        if (logFile) std::fclose(logFile);
    };
}
