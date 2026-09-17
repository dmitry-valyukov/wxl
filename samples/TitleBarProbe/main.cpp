// Проба своего заголовка окна на своём HWND.
//
// Окно устроено как wxl::CompositionWindow: HWND с WS_EX_NOREDIRECTIONBITMAP,
// сцена на DesktopAttachedSiteBridge и XAML островом DesktopWindowXamlSource.
// Заголовок ставится теми же публичными вызовами, какими его ставит
// Microsoft.UI.Xaml.Window: AppWindowTitleBar.ExtendsContentIntoTitleBar и
// InputNonClientPointerSource.SetRegionRects. Всё, что видно по ходу, пишется в
// titlebar-probe.log рядом с исполняемым файлом.
//
// Ключи:
//   --height standard|tall|collapsed   PreferredHeightOption (tall)
//   --buttons system|own                чьи кнопки окна (system)
//   --bar titlebar|grid                 контрол TitleBar или простая сетка
//   --caption element|none              ставить ли Caption по панели (element)
//   --zoom <f>                          OverrideScale моста острова на старте
//   --style fluent|caption|caption-red  свои кнопки: значки Segoe Fluent Icons
//                                       или встроенный стиль WindowCaptionButton
//
// Кнопки в теле окна (их находит драйвер по имени): «Крупнее», «Мельче»,
// «Масштаб 1», «Журнал», «Панель», «Высота».

#include <winrt/Microsoft.UI.Composition.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Windows.UI.h>

#include <windows.h>
#include <shellapi.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include "launch.h"

namespace {

namespace mu = winrt::Microsoft::UI;
namespace muc = winrt::Microsoft::UI::Composition;
namespace content = winrt::Microsoft::UI::Content;
namespace input = winrt::Microsoft::UI::Input;
namespace windowing = winrt::Microsoft::UI::Windowing;
namespace xaml = winrt::Microsoft::UI::Xaml;
namespace controls = winrt::Microsoft::UI::Xaml::Controls;
using winrt::Windows::Graphics::RectInt32;

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

std::string narrow(std::wstring_view text) {
    if (text.empty()) return {};
    int const size = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), size,
                          nullptr, nullptr);
    return out;
}

const char* kindName(input::NonClientRegionKind kind) {
    switch (kind) {
        case input::NonClientRegionKind::Close: return "Close";
        case input::NonClientRegionKind::Maximize: return "Maximize";
        case input::NonClientRegionKind::Minimize: return "Minimize";
        case input::NonClientRegionKind::Icon: return "Icon";
        case input::NonClientRegionKind::Caption: return "Caption";
        case input::NonClientRegionKind::TopBorder: return "TopBorder";
        case input::NonClientRegionKind::LeftBorder: return "LeftBorder";
        case input::NonClientRegionKind::BottomBorder: return "BottomBorder";
        case input::NonClientRegionKind::RightBorder: return "RightBorder";
        case input::NonClientRegionKind::Passthrough: return "Passthrough";
    }
    return "?";
}

constexpr input::NonClientRegionKind kAllKinds[] = {
    input::NonClientRegionKind::Close,       input::NonClientRegionKind::Maximize,
    input::NonClientRegionKind::Minimize,    input::NonClientRegionKind::Icon,
    input::NonClientRegionKind::Caption,     input::NonClientRegionKind::TopBorder,
    input::NonClientRegionKind::LeftBorder,  input::NonClientRegionKind::BottomBorder,
    input::NonClientRegionKind::RightBorder, input::NonClientRegionKind::Passthrough,
};

const char* heightName(windowing::TitleBarHeightOption option) {
    switch (option) {
        case windowing::TitleBarHeightOption::Standard: return "Standard";
        case windowing::TitleBarHeightOption::Tall: return "Tall";
        case windowing::TitleBarHeightOption::Collapsed: return "Collapsed";
    }
    return "?";
}

struct Options {
    windowing::TitleBarHeightOption height = windowing::TitleBarHeightOption::Tall;
    bool ownButtons = false;
    bool titleBarControl = true;
    bool captionByElement = true;
    float zoom = 0.0f;
    float sceneZoom = 0.0f;
    bool captionStyle = false;
    bool closeRed = false;
};

Options parseOptions() {
    Options options;
    int count = 0;
    wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &count);
    for (int i = 1; i < count; ++i) {
        std::wstring_view const key = argv[i];
        std::wstring_view const value = i + 1 < count ? argv[i + 1] : L"";
        if (key == L"--height") {
            options.height = value == L"standard"    ? windowing::TitleBarHeightOption::Standard
                             : value == L"collapsed" ? windowing::TitleBarHeightOption::Collapsed
                                                     : windowing::TitleBarHeightOption::Tall;
            ++i;
        } else if (key == L"--buttons") {
            options.ownButtons = value == L"own";
            ++i;
        } else if (key == L"--bar") {
            options.titleBarControl = value != L"grid";
            ++i;
        } else if (key == L"--caption") {
            options.captionByElement = value != L"none";
            ++i;
        } else if (key == L"--zoom") {
            options.zoom = std::wcstof(std::wstring(value).c_str(), nullptr);
            ++i;
        } else if (key == L"--scene-zoom") {
            options.sceneZoom = std::wcstof(std::wstring(value).c_str(), nullptr);
            ++i;
        } else if (key == L"--style") {
            options.captionStyle = value.starts_with(L"caption");
            options.closeRed = value == L"caption-red";
            ++i;
        }
    }
    ::LocalFree(argv);
    return options;
}

struct Probe {
    Options options;
    HWND hwnd = nullptr;
    mu::WindowId id{};

    muc::Compositor compositor{nullptr};
    muc::SpriteVisual backdrop{nullptr};
    content::ContentIsland sceneIsland{nullptr};
    content::DesktopAttachedSiteBridge sceneBridge{nullptr};
    input::InputPointerSource scenePointer{nullptr};

    windowing::AppWindow appWindow{nullptr};
    input::InputNonClientPointerSource nonClient{nullptr};

    xaml::Hosting::DesktopWindowXamlSource xamlSource{nullptr};
    controls::Grid root{nullptr};
    xaml::FrameworkElement bar{nullptr};
    controls::Button minimizeButton{nullptr};
    controls::Button maximizeButton{nullptr};
    controls::Button closeButton{nullptr};
    controls::FontIcon maximizeGlyph{nullptr};
    controls::TextBlock status{nullptr};

    float zoom = 1.0f;
    LRESULT lastHitTest = -1;
    bool regionsQueued = false;
};

Probe* probe = nullptr;

RectInt32 physicalRectOf(xaml::FrameworkElement const& element, float scale) {
    if (!element || !element.XamlRoot() || element.ActualWidth() <= 0 || element.ActualHeight() <= 0)
        return {};
    auto const bounds = element.TransformToVisual(nullptr).TransformBounds(
        {0.0f, 0.0f, static_cast<float>(element.ActualWidth()), static_cast<float>(element.ActualHeight())});
    return {static_cast<int32_t>(std::lround(bounds.X * scale)),
            static_cast<int32_t>(std::lround(bounds.Y * scale)),
            static_cast<int32_t>(std::lround(bounds.Width * scale)),
            static_cast<int32_t>(std::lround(bounds.Height * scale))};
}

void logRegions(const char* when) {
    if (!probe->nonClient) return;
    for (auto const kind : kAllKinds) {
        auto const rects = probe->nonClient.GetRegionRects(kind);
        if (rects.size() == 0) continue;
        std::string line;
        for (auto const& r : rects) {
            char item[64];
            std::snprintf(item, sizeof item, " [%d,%d %dx%d]", r.X, r.Y, r.Width, r.Height);
            line += item;
        }
        say("regions(%s) %s:%s", when, kindName(kind), line.c_str());
    }
}

BOOL CALLBACK logWindow(HWND window, LPARAM depth) {
    wchar_t cls[128] = {};
    wchar_t text[128] = {};
    ::GetClassNameW(window, cls, 128);
    ::GetWindowTextW(window, text, 128);
    RECT r{};
    ::GetWindowRect(window, &r);
    say("  %*swindow %p class='%s' text='%s' rect=[%ld,%ld %ldx%ld] visible=%d parent=%p owner=%p ex=%08lx",
        static_cast<int>(depth), "", static_cast<void*>(window), narrow(cls).c_str(), narrow(text).c_str(),
        r.left, r.top, r.right - r.left, r.bottom - r.top, ::IsWindowVisible(window) ? 1 : 0,
        static_cast<void*>(::GetParent(window)), static_cast<void*>(::GetWindow(window, GW_OWNER)),
        static_cast<unsigned long>(::GetWindowLongPtrW(window, GWL_EXSTYLE)));
    return TRUE;
}

void logWindows() {
    say("thread windows:");
    ::EnumThreadWindows(::GetCurrentThreadId(), logWindow, 0);
    say("children of main window:");
    ::EnumChildWindows(probe->hwnd, logWindow, 2);
}

void logMetrics(const char* when) {
    RECT window{};
    RECT client{};
    ::GetWindowRect(probe->hwnd, &window);
    ::GetClientRect(probe->hwnd, &client);
    RECT adjusted{0, 0, client.right, client.bottom};
    ::AdjustWindowRectExForDpi(&adjusted, static_cast<DWORD>(::GetWindowLongPtrW(probe->hwnd, GWL_STYLE)), FALSE,
                               static_cast<DWORD>(::GetWindowLongPtrW(probe->hwnd, GWL_EXSTYLE)),
                               ::GetDpiForWindow(probe->hwnd));
    // Рамка, которую окно само отвечает на WM_NCCALCSIZE с wParam = FALSE: так
    // её меряет CompositionWindow, и после ExtendsContentIntoTitleBar ответ
    // должен совпасть с живой рамкой, а не с AdjustWindowRectEx.
    RECT asked = window;
    ::SendMessageW(probe->hwnd, WM_NCCALCSIZE, FALSE, reinterpret_cast<LPARAM>(&asked));
    say("metrics(%s): dpi=%u window=%ldx%ld client=%ldx%ld live-frame=%ldx%ld adjust-frame=%ldx%ld "
        "nccalc-frame=%ld,%ld,%ld,%ld zoomed=%d",
        when, ::GetDpiForWindow(probe->hwnd), window.right - window.left, window.bottom - window.top,
        client.right, client.bottom, (window.right - window.left) - client.right,
        (window.bottom - window.top) - client.bottom, (adjusted.right - adjusted.left) - client.right,
        (adjusted.bottom - adjusted.top) - client.bottom, asked.left - window.left, asked.top - window.top,
        window.right - asked.right, window.bottom - asked.bottom, ::IsZoomed(probe->hwnd) ? 1 : 0);

    auto const titleBar = probe->appWindow.TitleBar();
    say("metrics(%s): AppWindowTitleBar extends=%d option=%s Height=%d LeftInset=%d RightInset=%d", when,
        titleBar.ExtendsContentIntoTitleBar() ? 1 : 0, heightName(titleBar.PreferredHeightOption()),
        titleBar.Height(), titleBar.LeftInset(), titleBar.RightInset());

    if (probe->root && probe->root.XamlRoot()) {
        auto const xamlRoot = probe->root.XamlRoot();
        auto const bridge = probe->xamlSource.SiteBridge();
        say("metrics(%s): XamlRoot.RasterizationScale=%.3f size=%.1fx%.1f bridge.OverrideScale=%.3f AppWindowId=%llx hwnd=%p",
            when, xamlRoot.RasterizationScale(), xamlRoot.Size().Width, xamlRoot.Size().Height,
            bridge.OverrideScale(), static_cast<unsigned long long>(xamlRoot.ContentIslandEnvironment().AppWindowId().Value),
            static_cast<void*>(probe->hwnd));
    }
    if (probe->bar && probe->bar.XamlRoot()) {
        auto const scale = static_cast<float>(probe->bar.XamlRoot().RasterizationScale());
        auto const r = physicalRectOf(probe->bar, scale);
        say("metrics(%s): bar logical=%.1fx%.1f physical=[%d,%d %dx%d]", when, probe->bar.ActualWidth(),
            probe->bar.ActualHeight(), r.X, r.Y, r.Width, r.Height);
    }
}

void updateStatus() {
    if (!probe->status || !probe->root.XamlRoot()) return;
    auto const titleBar = probe->appWindow.TitleBar();
    wchar_t text[256];
    std::swprintf(text, 256, L"dpi %u · XamlRoot %.2f · OverrideScale %.2f · %hs · Height %d · insets %d/%d",
                  ::GetDpiForWindow(probe->hwnd), probe->root.XamlRoot().RasterizationScale(),
                  probe->xamlSource.SiteBridge().OverrideScale(), heightName(titleBar.PreferredHeightOption()),
                  titleBar.Height(), titleBar.LeftInset(), titleBar.RightInset());
    probe->status.Text(text);
}

// То, что делает Window.SetTitleBar (CWindowChrome::SetDragRegion): Caption --
// прямоугольник элемента в пикселях клиента. Масштаб здесь -- XamlRoot, а не
// DPI окна: при OverrideScale это разные числа, и журнал показывает оба.
void updateRegions() {
    probe->regionsQueued = false;
    if (!probe->root || !probe->root.XamlRoot()) return;

    auto const xamlScale = static_cast<float>(probe->root.XamlRoot().RasterizationScale());
    auto const dpiScale = static_cast<float>(::GetDpiForWindow(probe->hwnd)) / 96.0f;
    say("updateRegions: xamlScale=%.3f dpiScale=%.3f", xamlScale, dpiScale);
    logRegions("before");

    // AppWindow сбрасывает Minimize/Maximize/Close к своим прямоугольникам на
    // каждое изменение места и активации -- ставим свои снова, но только если
    // они и правда другие: RegionsChanged приходит и на запись тех же чисел, и
    // без сравнения запись зациклилась бы.
    auto const apply = [](input::NonClientRegionKind kind, RectInt32 wanted) {
        auto const current = probe->nonClient.GetRegionRects(kind);
        bool const same = current.size() == 1 && current[0].X == wanted.X && current[0].Y == wanted.Y &&
                          current[0].Width == wanted.Width && current[0].Height == wanted.Height;
        if (!same) probe->nonClient.SetRegionRects(kind, {wanted});
    };

    if (probe->options.captionByElement) {
        bool const shown = probe->bar.Visibility() == xaml::Visibility::Visible;
        auto const rect = shown ? physicalRectOf(probe->bar, xamlScale) : RectInt32{};
        if (shown && rect.Width > 0)
            apply(input::NonClientRegionKind::Caption, rect);
        else if (probe->nonClient.GetRegionRects(input::NonClientRegionKind::Caption).size() != 0)
            probe->nonClient.ClearRegionRects(input::NonClientRegionKind::Caption);
    }

    if (probe->options.ownButtons) {
        apply(input::NonClientRegionKind::Minimize, physicalRectOf(probe->minimizeButton, xamlScale));
        apply(input::NonClientRegionKind::Maximize, physicalRectOf(probe->maximizeButton, xamlScale));
        apply(input::NonClientRegionKind::Close, physicalRectOf(probe->closeButton, xamlScale));
    }

    logRegions("after");
    updateStatus();
}

void queueRegions() {
    if (probe->regionsQueued) return;
    probe->regionsQueued = true;
    winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread().TryEnqueue(
        winrt::Microsoft::UI::Dispatching::DispatcherQueuePriority::Low, [] { updateRegions(); });
}

void setZoom(float zoom) {
    probe->zoom = zoom;
    probe->xamlSource.SiteBridge().OverrideScale(zoom);
    say("OverrideScale <- %.3f (read back %.3f)", zoom, probe->xamlSource.SiteBridge().OverrideScale());
}

winrt::Windows::Foundation::IInspectable appResource(const wchar_t* key) {
    return xaml::Application::Current().Resources().Lookup(winrt::box_value(winrt::hstring(key)));
}

// Встроенный стиль WindowCaptionButton берёт значок из Content как геометрию
// (Path.Data="{TemplateBinding Content}"), а у кнопки «развернуть» -- из своих
// состояний WindowStateNormal/WindowStateMaximized. Красные кисти закрытия
// (CloseButtonBackgroundPointerOver и соседи) в словарях тем есть, но шаблон
// их не называет: его состояния CloseButtonPointerOver/CloseButtonPressed
// берут WindowCaptionButtonBackgroundPointerOver -- её и подменяет caption-red.
controls::Button captionButton(xaml::Controls::FontIcon const& glyph, const wchar_t* geometry,
                               std::wstring_view name, bool close = false) {
    controls::Button button;
    if (probe->options.captionStyle) {
        button.Style(appResource(L"WindowCaptionButton").as<xaml::Style>());
        if (geometry)
            button.Content(xaml::Markup::XamlBindingHelper::ConvertValue(
                winrt::xaml_typename<xaml::Media::Geometry>(), winrt::box_value(geometry)));
        if (close && probe->options.closeRed) {
            for (auto const [target, source] : {
                     std::pair{L"WindowCaptionButtonBackgroundPointerOver", L"CloseButtonBackgroundPointerOver"},
                     std::pair{L"WindowCaptionButtonStrokePointerOver", L"CloseButtonStrokePointerOver"},
                     std::pair{L"WindowCaptionButtonBackgroundPressed", L"CloseButtonBackgroundPressed"},
                     std::pair{L"WindowCaptionButtonStrokePressed", L"CloseButtonStrokePressed"}}) {
                try {
                    button.Resources().Insert(winrt::box_value(winrt::hstring(target)), appResource(source));
                    say("close resource %s <- %s", narrow(target).c_str(), narrow(source).c_str());
                } catch (winrt::hresult_error const& error) {
                    say("close resource %s: lookup of %s failed %08x", narrow(target).c_str(),
                        narrow(source).c_str(), static_cast<unsigned>(error.code()));
                }
            }
        }
    } else {
        button.Content(glyph);
        button.CornerRadius({0, 0, 0, 0});
        button.BorderThickness({0, 0, 0, 0});
        button.Background(xaml::Media::SolidColorBrush(winrt::Windows::UI::Color{0, 0, 0, 0}));
    }
    button.Width(46);
    button.Height(std::numeric_limits<double>::quiet_NaN());
    button.VerticalAlignment(xaml::VerticalAlignment::Stretch);
    xaml::Automation::AutomationProperties::SetName(button, winrt::hstring(name));
    std::string const label = narrow(name);
    button.Click([label](auto&&, auto&&) { say("XAML Click on caption button '%s'", label.c_str()); });
    return button;
}

controls::FontIcon glyphOf(wchar_t code) {
    controls::FontIcon icon;
    icon.FontFamily(xaml::Media::FontFamily(L"Segoe Fluent Icons"));
    icon.FontSize(10);
    icon.Glyph(std::wstring(1, code));
    return icon;
}

controls::Button bodyButton(std::wstring_view text, std::function<void()> action) {
    controls::Button button;
    button.Content(winrt::box_value(winrt::hstring(text)));
    button.Margin({4, 4, 4, 4});
    xaml::Automation::AutomationProperties::SetName(button, winrt::hstring(text));
    button.Click([action = std::move(action)](auto&&, auto&&) { action(); });
    return button;
}

controls::Button barButton(std::wstring_view text) {
    controls::Button button;
    button.Content(winrt::box_value(winrt::hstring(text)));
    button.Margin({4, 0, 4, 0});
    button.VerticalAlignment(xaml::VerticalAlignment::Center);
    xaml::Automation::AutomationProperties::SetName(button, winrt::hstring(text));
    std::string const label = narrow(text);
    button.Click([label](auto&&, auto&&) { say("XAML Click on bar button '%s'", label.c_str()); });
    return button;
}

xaml::FrameworkElement makeBar() {
    controls::StackPanel left;
    left.Orientation(controls::Orientation::Horizontal);
    left.Children().Append(barButton(L"Вперёд"));

    controls::StackPanel middle;
    middle.Orientation(controls::Orientation::Horizontal);
    middle.VerticalAlignment(xaml::VerticalAlignment::Center);
    controls::TextBlock crumbs;
    crumbs.Text(L"WinAPI / Длинное название темы");
    crumbs.VerticalAlignment(xaml::VerticalAlignment::Center);
    crumbs.Margin({8, 0, 8, 0});
    middle.Children().Append(crumbs);
    middle.Children().Append(barButton(L"Кнопка в середине"));

    controls::StackPanel right;
    right.Orientation(controls::Orientation::Horizontal);
    right.Children().Append(barButton(L"Обновить"));
    right.Children().Append(barButton(L"О программе"));

    if (probe->options.titleBarControl) {
        controls::TitleBar titleBar;
        titleBar.Title(L"Беседка");
        titleBar.Subtitle(L"проба");
        controls::FontIconSource icon;
        icon.Glyph(std::wstring(1, wchar_t{0xE8BD}));
        titleBar.IconSource(icon);
        titleBar.IsBackButtonVisible(true);
        titleBar.BackRequested([](auto&&, auto&&) { say("TitleBar.BackRequested"); });
        titleBar.LeftHeader(left);
        titleBar.Content(middle);
        titleBar.RightHeader(right);
        return titleBar;
    }

    controls::Grid grid;
    grid.Height(48);
    for (auto const width : {xaml::GridLengthHelper::Auto(), xaml::GridLengthHelper::Auto(),
                             xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star),
                             xaml::GridLengthHelper::Auto()}) {
        controls::ColumnDefinition column;
        column.Width(width);
        grid.ColumnDefinitions().Append(column);
    }
    controls::TextBlock name;
    name.Text(L"Беседка");
    name.VerticalAlignment(xaml::VerticalAlignment::Center);
    name.Margin({12, 0, 12, 0});
    grid.Children().Append(name);
    controls::Grid::SetColumn(left, 1);
    grid.Children().Append(left);
    controls::Grid::SetColumn(middle, 2);
    grid.Children().Append(middle);
    controls::Grid::SetColumn(right, 3);
    grid.Children().Append(right);
    return grid;
}

void setCaptionState(input::NonClientRegionKind kind, const wchar_t* state) {
    controls::Button button{nullptr};
    switch (kind) {
        case input::NonClientRegionKind::Minimize: button = probe->minimizeButton; break;
        case input::NonClientRegionKind::Maximize: button = probe->maximizeButton; break;
        case input::NonClientRegionKind::Close: button = probe->closeButton; break;
        default: return;
    }
    if (!button) return;
    std::wstring name{state};
    if (kind == input::NonClientRegionKind::Close && probe->options.captionStyle && name != L"Normal")
        name = L"CloseButton" + name;
    xaml::VisualStateManager::GoToState(button, name, false);
}

void setMaximizedState(bool maximized) {
    if (probe->maximizeGlyph)
        probe->maximizeGlyph.Glyph(std::wstring(1, maximized ? wchar_t{0xE923} : wchar_t{0xE922}));
    if (probe->maximizeButton && probe->options.captionStyle)
        xaml::VisualStateManager::GoToState(probe->maximizeButton,
                                            maximized ? L"WindowStateMaximized" : L"WindowStateNormal", false);
}

void buildContent() {
    probe->root = controls::Grid{};
    for (auto const height : {xaml::GridLengthHelper::Auto(),
                              xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star)}) {
        controls::RowDefinition row;
        row.Height(height);
        probe->root.RowDefinitions().Append(row);
    }

    // Строка заголовка: панель во всю ширину, свои кнопки окна -- справа от неё,
    // вне контрола TitleBar, чтобы их области не накрывались его Passthrough.
    controls::Grid titleRow;
    titleRow.Background(xaml::Media::SolidColorBrush(winrt::Windows::UI::Color{0xFF, 0xE6, 0xEC, 0xF2}));
    for (auto const width : {xaml::GridLengthHelper::FromValueAndType(1, xaml::GridUnitType::Star),
                             xaml::GridLengthHelper::Auto()}) {
        controls::ColumnDefinition column;
        column.Width(width);
        titleRow.ColumnDefinitions().Append(column);
    }
    probe->bar = makeBar();
    titleRow.Children().Append(probe->bar);

    if (probe->options.ownButtons) {
        controls::StackPanel buttons;
        buttons.Orientation(controls::Orientation::Horizontal);
        probe->minimizeButton = captionButton(glyphOf(0xE921), L"M 0 0 L 10 0", L"Свернуть");
        if (!probe->options.captionStyle) probe->maximizeGlyph = glyphOf(0xE922);
        probe->maximizeButton = captionButton(probe->maximizeGlyph, nullptr, L"Развернуть");
        probe->closeButton = captionButton(glyphOf(0xE8BB), L"M 0 0 L 9 9 M 9 0 L 0 9", L"Закрыть", true);
        probe->maximizeButton.Loaded([](auto&&, auto&&) { setMaximizedState(::IsZoomed(probe->hwnd) != 0); });
        buttons.Children().Append(probe->minimizeButton);
        buttons.Children().Append(probe->maximizeButton);
        buttons.Children().Append(probe->closeButton);
        controls::Grid::SetColumn(buttons, 1);
        titleRow.Children().Append(buttons);
        probe->minimizeButton.SizeChanged([](auto&&, auto&&) { queueRegions(); });
    }
    probe->root.Children().Append(titleRow);

    controls::StackPanel body;
    body.Margin({24, 24, 24, 24});
    controls::Grid::SetRow(body, 1);
    probe->status = controls::TextBlock{};
    probe->status.Foreground(xaml::Media::SolidColorBrush(winrt::Windows::UI::Color{0xFF, 0xFF, 0xFF, 0xFF}));
    body.Children().Append(probe->status);

    controls::StackPanel actions;
    actions.Orientation(controls::Orientation::Horizontal);
    actions.Children().Append(bodyButton(L"Крупнее", [] { setZoom(probe->zoom * 1.25f); }));
    actions.Children().Append(bodyButton(L"Мельче", [] { setZoom(probe->zoom / 1.25f); }));
    actions.Children().Append(bodyButton(L"Масштаб 1", [] { setZoom(1.0f); }));
    actions.Children().Append(bodyButton(L"Журнал", [] {
        logMetrics("button");
        logRegions("button");
        logWindows();
    }));
    actions.Children().Append(bodyButton(L"Панель", [] {
        auto const shown = probe->bar.Visibility() == xaml::Visibility::Visible;
        probe->bar.Visibility(shown ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
        say("bar visibility -> %s", shown ? "Collapsed" : "Visible");
        queueRegions();
    }));
    actions.Children().Append(bodyButton(L"Высота", [] {
        auto const titleBar = probe->appWindow.TitleBar();
        auto const next = static_cast<windowing::TitleBarHeightOption>(
            (static_cast<int>(titleBar.PreferredHeightOption()) + 1) % 3);
        titleBar.PreferredHeightOption(next);
        say("PreferredHeightOption -> %s, Height=%d", heightName(next), titleBar.Height());
        queueRegions();
    }));
    body.Children().Append(actions);
    probe->root.Children().Append(body);

    probe->bar.SizeChanged([](auto&&, auto&&) { queueRegions(); });
    probe->root.Loaded([](auto&&, auto&&) {
        say("root Loaded");
        probe->root.XamlRoot().Changed([](xaml::XamlRoot const& sender, auto&&) {
            say("XamlRoot.Changed: RasterizationScale=%.3f size=%.1fx%.1f", sender.RasterizationScale(),
                sender.Size().Width, sender.Size().Height);
            queueRegions();
        });
        logMetrics("loaded");
        queueRegions();
    });
}

void resize() {
    RECT client{};
    ::GetClientRect(probe->hwnd, &client);
    if (probe->backdrop)
        probe->backdrop.Size({static_cast<float>(client.right), static_cast<float>(client.bottom)});
    if (probe->xamlSource) probe->xamlSource.SiteBridge().MoveAndResize({0, 0, client.right, client.bottom});
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (!probe) return ::DefWindowProcW(hwnd, message, wparam, lparam);
    switch (message) {
        case WM_ERASEBKGND:
            return 1;
        case WM_NCCALCSIZE: {
            LRESULT const result = ::DefWindowProcW(hwnd, message, wparam, lparam);
            if (wparam) {
                auto const* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
                say("WM_NCCALCSIZE reached probe: client=[%ld,%ld,%ld,%ld]", params->rgrc[0].left,
                    params->rgrc[0].top, params->rgrc[0].right, params->rgrc[0].bottom);
            }
            return result;
        }
        case WM_NCHITTEST: {
            LRESULT const result = ::DefWindowProcW(hwnd, message, wparam, lparam);
            if (result != probe->lastHitTest) {
                say("WM_NCHITTEST reached probe: %lld at %d,%d", static_cast<long long>(result),
                    static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam)));
                probe->lastHitTest = result;
            }
            return result;
        }
        case WM_SYSCOMMAND:
            say("WM_SYSCOMMAND %04llx", static_cast<unsigned long long>(wparam & 0xFFF0));
            break;
        // Какие неклиентские сообщения мыши доходят до самого окна, а не до
        // его дочерних окон: над островом верхнее окно их не получает.
        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONUP:
        case WM_NCLBUTTONDBLCLK:
        case WM_NCRBUTTONDOWN:
        case WM_NCRBUTTONUP:
            say("message %04x reached probe: ht=%llu at %d,%d", message, static_cast<unsigned long long>(wparam),
                static_cast<short>(LOWORD(lparam)), static_cast<short>(HIWORD(lparam)));
            break;
        case WM_NCMOUSEMOVE:
            if (static_cast<LRESULT>(wparam) != probe->lastHitTest) {
                say("WM_NCMOUSEMOVE reached probe: ht=%llu", static_cast<unsigned long long>(wparam));
                probe->lastHitTest = static_cast<LRESULT>(wparam);
            }
            break;
        case WM_ACTIVATE:
            say("WM_ACTIVATE %llu", static_cast<unsigned long long>(LOWORD(wparam)));
            queueRegions();
            break;
        case WM_SIZE:
            if (wparam != SIZE_MINIMIZED) {
                resize();
                setMaximizedState(wparam == SIZE_MAXIMIZED);
                queueRegions();
            }
            break;
        case WM_DPICHANGED: {
            auto const* suggested = reinterpret_cast<RECT*>(lparam);
            say("WM_DPICHANGED %u", LOWORD(wparam));
            ::SetWindowPos(hwnd, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                           suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_CLOSE:
            say("WM_CLOSE");
            ::DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            xaml::Application::Current().Exit();
            return 0;
    }
    return ::DefWindowProcW(hwnd, message, wparam, lparam);
}

void subscribeNonClient() {
    auto& nc = probe->nonClient;
    nc.RegionsChanged([](auto&&, input::NonClientRegionsChangedEventArgs const& args) {
        std::string kinds;
        for (auto const kind : args.ChangedRegions()) {
            kinds += ' ';
            kinds += kindName(kind);
        }
        say("RegionsChanged:%s", kinds.c_str());
        if (probe->options.ownButtons) queueRegions();
    });
    auto const report = [](const char* what, input::NonClientPointerEventArgs const& args) {
        say("NonClient %s: kind=%s point=%.0f,%.0f inRegion=%d", what, kindName(args.RegionKind()), args.Point().X,
            args.Point().Y, args.IsPointInRegion() ? 1 : 0);
    };
    nc.PointerEntered([report](auto&&, input::NonClientPointerEventArgs const& args) {
        report("PointerEntered", args);
        setCaptionState(args.RegionKind(), L"PointerOver");
    });
    nc.PointerExited([report](auto&&, input::NonClientPointerEventArgs const& args) {
        report("PointerExited", args);
        setCaptionState(args.RegionKind(), L"Normal");
    });
    nc.PointerPressed([report](auto&&, input::NonClientPointerEventArgs const& args) {
        report("PointerPressed", args);
        setCaptionState(args.RegionKind(), L"Pressed");
    });
    nc.PointerReleased([report](auto&&, input::NonClientPointerEventArgs const& args) {
        report("PointerReleased", args);
        setCaptionState(args.RegionKind(), L"PointerOver");
    });
    nc.CaptionTapped([](auto&&, input::NonClientCaptionTappedEventArgs const& args) {
        say("NonClient CaptionTapped at %.0f,%.0f", args.Point().X, args.Point().Y);
    });
}

}  // namespace

wxl::Teardown wxl_launched() {
    wchar_t path[MAX_PATH] = {};
    ::GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring logPath{path};
    logPath.resize(logPath.find_last_of(L'\\') + 1);
    logPath += L"titlebar-probe.log";
    logFile = ::_wfopen(logPath.c_str(), L"w");
    startTick = ::GetTickCount64();

    probe = new Probe{};
    probe->options = parseOptions();
    say("options: height=%s buttons=%s bar=%s caption=%s zoom=%.3f style=%s", heightName(probe->options.height),
        probe->options.ownButtons ? "own" : "system", probe->options.titleBarControl ? "titlebar" : "grid",
        probe->options.captionByElement ? "element" : "none", probe->options.zoom,
        probe->options.closeRed ? "caption-red" : probe->options.captionStyle ? "caption" : "fluent");

    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = ::GetModuleHandleW(nullptr);
    wc.lpszClassName = L"wxl.TitleBarProbe";
    wc.hCursor = ::LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    ::RegisterClassExW(&wc);

    probe->hwnd = ::CreateWindowExW(WS_EX_NOREDIRECTIONBITMAP, wc.lpszClassName, L"Проба заголовка",
                                    WS_OVERLAPPEDWINDOW, 200, 120, 1100, 640, nullptr, nullptr,
                                    wc.hInstance, nullptr);
    probe->id = mu::WindowId{static_cast<uint64_t>(reinterpret_cast<uintptr_t>(probe->hwnd))};

    // Сцена -- как в CompositionWindow: один визуал с заливкой, свой остров на
    // attached-мосту и источник указателя этого острова.
    probe->compositor = muc::Compositor{};
    probe->backdrop = probe->compositor.CreateSpriteVisual();
    probe->backdrop.Brush(probe->compositor.CreateColorBrush(winrt::Windows::UI::Color{0xFF, 0x1E, 0x3A, 0x3A}));
    probe->sceneIsland = content::ContentIsland::Create(probe->backdrop);
    probe->sceneBridge = content::DesktopAttachedSiteBridge::CreateFromWindowId(
        winrt::Microsoft::UI::Dispatching::DispatcherQueue::GetForCurrentThread(), probe->id);
    probe->sceneBridge.Connect(probe->sceneIsland);
    if (probe->options.sceneZoom > 0.0f) {
        probe->sceneBridge.OverrideScale(probe->options.sceneZoom);
        say("scene bridge OverrideScale <- %.3f, island RasterizationScale=%.3f", probe->options.sceneZoom,
            probe->sceneIsland.RasterizationScale());
    }
    probe->scenePointer = input::InputPointerSource::GetForIsland(probe->sceneIsland);
    probe->scenePointer.PointerPressed([](auto&&, input::PointerEventArgs const& args) {
        say("scene PointerPressed at %.0f,%.0f", args.CurrentPoint().Position().X, args.CurrentPoint().Position().Y);
    });

    // Заголовок -- как CWindowChrome::ConfigureWindowChrome: расширить
    // содержимое в заголовок, фон кнопок прозрачный, затем SWP_FRAMECHANGED.
    probe->appWindow = windowing::AppWindow::GetFromWindowId(probe->id);
    auto const titleBar = probe->appWindow.TitleBar();
    titleBar.ExtendsContentIntoTitleBar(true);
    titleBar.PreferredHeightOption(probe->options.ownButtons ? windowing::TitleBarHeightOption::Collapsed
                                                             : probe->options.height);
    winrt::Windows::Foundation::IReference<winrt::Windows::UI::Color> const transparent{
        winrt::Windows::UI::Color{0, 0xFF, 0xFF, 0xFF}};
    titleBar.ButtonBackgroundColor(transparent);
    titleBar.ButtonInactiveBackgroundColor(transparent);
    probe->nonClient = input::InputNonClientPointerSource::GetForWindowId(probe->id);
    subscribeNonClient();

    RECT window{};
    ::GetWindowRect(probe->hwnd, &window);
    ::SetWindowPos(probe->hwnd, nullptr, window.left, window.top, window.right - window.left,
                   window.bottom - window.top, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOZORDER);

    buildContent();
    probe->xamlSource = xaml::Hosting::DesktopWindowXamlSource{};
    probe->xamlSource.Initialize(probe->id);
    probe->xamlSource.Content(probe->root);
    if (probe->options.zoom > 0.0f) setZoom(probe->options.zoom);
    resize();
    probe->xamlSource.SiteBridge().Show();

    ::ShowWindow(probe->hwnd, SW_SHOW);
    ::UpdateWindow(probe->hwnd);
    logMetrics("shown");
    logWindows();

    return [](wxl::Reason) {
        say("teardown");
        if (logFile) std::fclose(logFile);
    };
}
