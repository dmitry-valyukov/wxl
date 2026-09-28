// Проба клавиш масштаба у окна с островом XAML.
//
// ZoomEffect ловит Ctrl с «+», «-» и «0» в PreviewKeyDown корня острова. В
// Беседке сразу после запуска эти клавиши не работают: фокус Windows стоит на
// самом окне верхнего уровня, клавиша приходит в его оконную процедуру и до
// XAML не доходит. Вопрос пользователя -- перехватывается ли клавиша на самом
// верхнем уровне окна. Проба выясняет, где её можно поймать при любом
// положении фокуса:
//
//   - оконная процедура окна верхнего уровня (WM_KEYDOWN);
//   - хуки потока интерфейса WH_GETMESSAGE и WH_KEYBOARD -- проходит ли
//     клавиша острова через очередь сообщений потока (колесо острова, по
//     пробе ZoomGestureProbe, мимо неё);
//   - источник клавиатуры острова (InputKeyboardSource::GetForIsland) --
//     раньше XAML или позже;
//   - PreviewKeyDown у верхнего элемента острова (ScrollViewer-датчик, как в
//     CompositionWindow) и у корня под ним, KeyDown с handledEventsToo;
//   - KeyboardAccelerator на корне;
//   - PreviewKeyDown у содержимого всплывающего Flyout.
//
// Положения фокуса -- этапы 1-10 в phases(); на этапе 10 окно при получении
// фокуса отдаёт его острову (NavigateFocus(Restore)).
//
// С ключом --window-like окно ведёт фокус, как Microsoft.UI.Xaml.Window
// (DesktopWindowImpl в исходниках WinUI 3, microsoft-ui-xaml): WM_ACTIVATE не
// отдаёт DefWindowProc, при активации отдаёт фокус острову и возвращает его
// элементу XAML, Tab за крайний элемент возвращает в остров. Этапы W1-W5 в
// windowLikePhases().
//
// С ключом --info-flyout (фокус -- как с --window-like) -- информационный
// flyout, как у ZoomEffect (Transient, текст «110 %» под кнопкой): берёт ли он
// фокус при показе, от щелчка и от Tab -- как есть и с запретами фокуса и
// попадания (этапы F1-F6 в infoFlyoutPhases()). Проба прогоняет их сама и
// закрывается; всё пишет в keyboard-zoom-probe.log рядом с исполняемым файлом.
// SendInput нажимает настоящие клавиши и щёлкает настоящей мышью: на время
// пробы клавиатура и мышь -- её.

#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.UI.Xaml.Interop.h>  // xaml_typename -- стиль FlyoutPresenter
#include <winrt/Windows.UI.h>

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "launch.h"

namespace {

namespace mu = winrt::Microsoft::UI;
namespace xaml = winrt::Microsoft::UI::Xaml;
namespace controls = winrt::Microsoft::UI::Xaml::Controls;
using winrt::Windows::System::VirtualKey;
using winrt::Windows::System::VirtualKeyModifiers;

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

std::string className(HWND hwnd) {
    if (!hwnd) return "(none)";
    wchar_t name[128] = {};
    ::GetClassNameW(hwnd, name, 128);
    return winrt::to_string(name);
}

struct Step {
    int offset;
    std::function<void()> run;
};

struct Phase {
    const char* name;
    std::vector<Step> steps;
};

struct Probe {
    HWND hwnd = nullptr;
    HWND other = nullptr;  // второе окно -- для этапа с уходом и возвратом активности
    xaml::Hosting::DesktopWindowXamlSource xamlSource{nullptr};
    controls::ScrollViewer sensor{nullptr};
    controls::Grid root{nullptr};
    controls::TextBox field{nullptr};
    controls::Button button{nullptr};
    controls::Flyout flyout{nullptr};
    controls::TextBox popupField{nullptr};
    winrt::Microsoft::UI::Input::InputKeyboardSource islandKeyboard{nullptr};
    HHOOK getMessageHook = nullptr;
    HHOOK keyboardHook = nullptr;

    std::vector<Phase> phases;
    std::vector<std::string> seen;  // кто увидел Ctrl+«+» на этапе, по порядку
    int step = 0;
    bool restoreFocusOnActivation = false;  // этап 10: WM_SETFOCUS окна -> NavigateFocus(Restore)

    // Режим --window-like: фокус ведётся, как у Microsoft.UI.Xaml.Window
    // (DesktopWindowImpl::OnActivate и OnDesktopWindowXamlSourceTakeFocusRequested).
    bool windowLike = false;
    HWND lastFocusedChild = nullptr;
    bool initialActivation = true;
    bool islandFocusPending = false;  // остров был недоступен, когда окно его звало
    winrt::guid lastTakeFocusCorrelation{};
};

Probe* probe = nullptr;

constexpr WPARAM plusKey = VK_OEM_PLUS;
constexpr int phaseTicks = 80;  // тики по 16 мс

void seen(std::string what) {
    say("  seen: %s", what.c_str());
    probe->seen.push_back(std::move(what));
}

bool isPlus(xaml::Input::KeyRoutedEventArgs const& args) { return args.Key() == static_cast<VirtualKey>(plusKey); }

std::string handledNote(bool handled) { return handled ? " (handled)" : ""; }

// ---- хуки потока ----

LRESULT CALLBACK onGetMessage(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION && wparam == PM_REMOVE) {
        auto const* message = reinterpret_cast<MSG const*>(lparam);
        if ((message->message == WM_KEYDOWN || message->message == WM_SYSKEYDOWN) && message->wParam == plusKey) {
            seen("WH_GETMESSAGE WM_KEYDOWN to " + className(message->hwnd));
        }
    }
    return ::CallNextHookEx(nullptr, code, wparam, lparam);
}

LRESULT CALLBACK onKeyboard(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION && wparam == plusKey && (lparam & 0x80000000) == 0) {
        seen("WH_KEYBOARD, focus " + className(::GetFocus()));
    }
    return ::CallNextHookEx(nullptr, code, wparam, lparam);
}

// ---- состояние фокуса ----

std::string xamlFocus() {
    auto const xamlRoot = probe->root.XamlRoot();
    if (!xamlRoot) return "(no XamlRoot)";
    auto const focused = xaml::Input::FocusManager::GetFocusedElement(xamlRoot);
    if (!focused) return "(none)";
    std::string text = winrt::to_string(winrt::get_class_name(focused));
    if (auto const element = focused.try_as<xaml::FrameworkElement>(); element && !element.Name().empty()) {
        text += " '" + winrt::to_string(element.Name()) + "'";
    }
    return text;
}

void describeFocus() {
    HWND const foreground = ::GetForegroundWindow();
    say("  focus: win32 %s; foreground %s; xaml %s", className(::GetFocus()).c_str(),
        foreground == probe->hwnd ? "probe window" : className(foreground).c_str(), xamlFocus().c_str());
}

HWND findDescendant(HWND parent, wchar_t const* wanted) {
    struct Search {
        wchar_t const* wanted;
        HWND found;
    } search{wanted, nullptr};
    ::EnumChildWindows(
        parent,
        [](HWND child, LPARAM param) -> BOOL {
            auto& s = *reinterpret_cast<Search*>(param);
            wchar_t name[128] = {};
            ::GetClassNameW(child, name, 128);
            if (::wcscmp(name, s.wanted) == 0) {
                s.found = child;
                return FALSE;
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&search));
    return search.found;
}

void listChildWindows() {
    ::EnumChildWindows(
        probe->hwnd,
        [](HWND child, LPARAM) -> BOOL {
            say("  child window %p class %s parent %p", static_cast<void*>(child), className(child).c_str(),
                static_cast<void*>(::GetParent(child)));
            return TRUE;
        },
        0);
}

// ---- фокус, как у Microsoft.UI.Xaml.Window ----

HWND bridgeWindow() {
    return reinterpret_cast<HWND>(static_cast<uintptr_t>(probe->xamlSource.SiteBridge().WindowId().Value));
}

bool islandOwns(HWND window) {
    HWND const bridge = bridgeWindow();
    return window == bridge || ::IsChild(bridge, window);
}

// DesktopWindowImpl::SetFocusToContentIsland -> XamlIslandRoot::TrySetFocus.
void setFocusToContentIsland() {
    auto const xamlRoot = probe->root.XamlRoot();
    if (!xamlRoot) {
        // Window берёт остров внутренним вызовом сразу после Initialize; снаружи
        // остров виден только через XamlRoot содержимого -- после его загрузки.
        say("  SetFocusToContentIsland: no XamlRoot yet, pending");
        probe->islandFocusPending = true;
        return;
    }
    probe->islandFocusPending = false;
    auto const controller = winrt::Microsoft::UI::Input::InputFocusController::GetForIsland(xamlRoot.ContentIsland());
    if (controller.HasFocus()) {
        say("  SetFocusToContentIsland: island already has focus");
        return;
    }
    say("  SetFocusToContentIsland: TrySetFocus=%d", controller.TrySetFocus() ? 1 : 0);
}

void restoreFocus() {
    auto const result = probe->xamlSource.NavigateFocus(
        xaml::Hosting::XamlSourceFocusNavigationRequest{xaml::Hosting::XamlSourceFocusNavigationReason::Restore});
    say("  RestoreFocus: WasFocusMoved=%d", result.WasFocusMoved() ? 1 : 0);
}

void onActivate(WPARAM wparam) {
    bool const minimized = HIWORD(wparam) != 0;
    WORD const state = LOWORD(wparam);
    say("  WM_ACTIVATE %s%s", state == WA_INACTIVE ? "WA_INACTIVE" : state == WA_CLICKACTIVE ? "WA_CLICKACTIVE" : "WA_ACTIVE",
        minimized ? " minimized" : "");
    if (state == WA_INACTIVE && !minimized) {
        HWND const focus = ::GetFocus();
        if (focus && ::IsChild(probe->hwnd, focus)) probe->lastFocusedChild = focus;
    }
    if (!minimized && state != WA_INACTIVE) {
        if (!probe->lastFocusedChild || islandOwns(probe->lastFocusedChild)) {
            setFocusToContentIsland();
            restoreFocus();
        } else {
            ::SetFocus(probe->lastFocusedChild);
        }
    }
    if (!minimized && probe->initialActivation) {
        setFocusToContentIsland();
        probe->initialActivation = false;
    }
}

void onTakeFocusRequested(xaml::Hosting::DesktopWindowXamlSourceTakeFocusRequestedEventArgs const& args) {
    auto const request = args.Request();
    auto const correlation = request.CorrelationId();
    auto const reason = request.Reason();
    say("  TakeFocusRequested reason=%d", static_cast<int>(reason));
    if (correlation == probe->lastTakeFocusCorrelation) {
        restoreFocus();
        return;
    }
    probe->lastTakeFocusCorrelation = correlation;
    if (reason == xaml::Hosting::XamlSourceFocusNavigationReason::First ||
        reason == xaml::Hosting::XamlSourceFocusNavigationReason::Last) {
        auto const result = probe->xamlSource.NavigateFocus(
            xaml::Hosting::XamlSourceFocusNavigationRequest{reason, winrt::Windows::Foundation::Rect{}, correlation});
        say("  NavigateFocus(%d): WasFocusMoved=%d", static_cast<int>(reason), result.WasFocusMoved() ? 1 : 0);
    }
}

// ---- ввод ----

void key(WORD vk, bool down) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    ::SendInput(1, &input, sizeof(INPUT));
}

void clickAt(POINT point) {
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

void clickEmptyIslandArea() {
    RECT client{};
    ::GetClientRect(probe->hwnd, &client);
    POINT point{client.right - 60, client.bottom - 60};
    ::ClientToScreen(probe->hwnd, &point);
    clickAt(point);
}

// ---- информационный flyout, как у ZoomEffect ----

enum class InfoFlyout { AsNow, NotFocusable, NotFocusableNotHitTestable };

controls::Flyout infoFlyout{nullptr};
controls::TextBlock infoText{nullptr};

void showInfoFlyout(InfoFlyout kind) {
    if (infoFlyout) infoFlyout.Hide();
    infoText = controls::TextBlock{};
    infoText.Text(L"110 %");
    infoText.PointerPressed([](auto&&, xaml::Input::PointerRoutedEventArgs const&) { say("  info text PointerPressed"); });
    infoFlyout = controls::Flyout{};
    infoFlyout.Content(infoText);
    infoFlyout.ShowMode(controls::Primitives::FlyoutShowMode::Transient);
    if (kind != InfoFlyout::AsNow) {
        // Ни щелчок, ни Tab не дают ему фокуса.
        infoFlyout.AllowFocusOnInteraction(false);
        xaml::Style presenter{winrt::xaml_typename<controls::FlyoutPresenter>()};
        presenter.Setters().Append(xaml::Setter{xaml::UIElement::IsTabStopProperty(), winrt::box_value(false)});
        presenter.Setters().Append(
            xaml::Setter{xaml::FrameworkElement::AllowFocusOnInteractionProperty(), winrt::box_value(false)});
        if (kind == InfoFlyout::NotFocusableNotHitTestable) {
            presenter.Setters().Append(xaml::Setter{xaml::UIElement::IsHitTestVisibleProperty(), winrt::box_value(false)});
        }
        infoFlyout.FlyoutPresenterStyle(presenter);
    }
    controls::Primitives::FlyoutShowOptions options;
    options.ShowMode(controls::Primitives::FlyoutShowMode::Transient);
    options.Placement(controls::Primitives::FlyoutPlacementMode::Bottom);
    infoFlyout.ShowAt(probe->button, options);
}

void clickInfoFlyout() {
    if (!infoText || !infoText.XamlRoot()) {
        say("  info flyout not in the tree");
        return;
    }
    auto const center = infoText.TransformToVisual(nullptr).TransformPoint(
        {static_cast<float>(infoText.ActualWidth() / 2), static_cast<float>(infoText.ActualHeight() / 2)});
    double const scale = infoText.XamlRoot().RasterizationScale();
    POINT point{static_cast<LONG>(center.X * scale), static_cast<LONG>(center.Y * scale)};
    ::ClientToScreen(probe->hwnd, &point);
    clickAt(point);
}

// ---- этапы ----

std::vector<Phase> phases() {
    return {
        {"1 startup: nothing after ShowWindow and SetForegroundWindow",
         {{0, [] { ::SetForegroundWindow(probe->hwnd); }}}},
        {"2 DesktopWindowXamlSource.NavigateFocus(Programmatic) from state 1",
         {{0, [] {
               auto const result = probe->xamlSource.NavigateFocus(xaml::Hosting::XamlSourceFocusNavigationRequest{
                   xaml::Hosting::XamlSourceFocusNavigationReason::Programmatic});
               say("  NavigateFocus: WasFocusMoved=%d", result.WasFocusMoved() ? 1 : 0);
           }}}},
        {"3 ::SetFocus(top-level window) -- back to the window", {{0, [] { ::SetFocus(probe->hwnd); }}}},
        {"4 ::SetFocus(InputSite child window) from state 3",
         {{0, [] {
               HWND const site = findDescendant(probe->hwnd, L"InputSiteWindowClass");
               say("  InputSite window %p", static_cast<void*>(site));
               if (site) ::SetFocus(site);
           }}}},
        {"5 TextBox.Focus(Programmatic)", {{0, [] { probe->field.Focus(xaml::FocusState::Programmatic); }}}},
        {"6 Button.Focus(Keyboard)", {{0, [] { probe->button.Focus(xaml::FocusState::Keyboard); }}}},
        {"7 mouse click on an empty area of the island", {{0, [] { clickEmptyIslandArea(); }}}},
        {"8 TextBox inside an open Flyout",
         {{0, [] { probe->flyout.ShowAt(probe->button); }},
          {15, [] { probe->popupField.Focus(xaml::FocusState::Programmatic); }}}},
        {"9 TextBox focused, another window activated, the probe window activated again",
         {{0, [] {
               probe->flyout.Hide();
               probe->field.Focus(xaml::FocusState::Programmatic);
           }},
          {8, [] { ::SetForegroundWindow(probe->other); }},
          {18, [] { ::SetForegroundWindow(probe->hwnd); }}}},
        {"10 as 9, but WM_SETFOCUS of the window calls NavigateFocus(Restore)",
         {{0, [] {
               probe->restoreFocusOnActivation = true;
               probe->button.Focus(xaml::FocusState::Programmatic);
           }},
          {8, [] { ::SetForegroundWindow(probe->other); }},
          {18, [] { ::SetForegroundWindow(probe->hwnd); }}}},
    };
}

std::vector<Phase> infoFlyoutPhases() {
    return {
        {"F1 TextBox focused, info flyout shown as ZoomEffect shows it now",
         {{0, [] { probe->field.Focus(xaml::FocusState::Programmatic); }},
          {4, [] { showInfoFlyout(InfoFlyout::AsNow); }}}},
        {"F2 click on the info flyout, as now",
         {{0, [] { probe->field.Focus(xaml::FocusState::Programmatic); }},
          {2, [] { showInfoFlyout(InfoFlyout::AsNow); }},
          {14, [] { clickInfoFlyout(); }}}},
        {"F3 click on the info flyout, not focusable (AllowFocusOnInteraction, IsTabStop false)",
         {{0, [] { probe->field.Focus(xaml::FocusState::Programmatic); }},
          {2, [] { showInfoFlyout(InfoFlyout::NotFocusable); }},
          {14, [] { clickInfoFlyout(); }}}},
        {"F4 click on the info flyout, not focusable and not hit-testable",
         {{0, [] { probe->field.Focus(xaml::FocusState::Programmatic); }},
          {2, [] { showInfoFlyout(InfoFlyout::NotFocusableNotHitTestable); }},
          {14, [] { clickInfoFlyout(); }}}},
        {"F5 Tab, Tab from the TextBox with the info flyout open, as now",
         {{0, [] { probe->field.Focus(xaml::FocusState::Keyboard); }},
          {2, [] { showInfoFlyout(InfoFlyout::AsNow); }},
          {8, [] { key(VK_TAB, true); }},
          {9, [] { key(VK_TAB, false); }},
          {14, [] { describeFocus(); }},
          {16, [] { key(VK_TAB, true); }},
          {17, [] { key(VK_TAB, false); }}}},
        {"F6 Tab, Tab from the TextBox with the info flyout open, not focusable and not hit-testable",
         {{0, [] { probe->field.Focus(xaml::FocusState::Keyboard); }},
          {2, [] { showInfoFlyout(InfoFlyout::NotFocusableNotHitTestable); }},
          {8, [] { key(VK_TAB, true); }},
          {9, [] { key(VK_TAB, false); }},
          {14, [] { describeFocus(); }},
          {16, [] { key(VK_TAB, true); }},
          {17, [] { key(VK_TAB, false); }}}},
    };
}

std::vector<Phase> windowLikePhases() {
    return {
        {"W1 startup: initial activation, nothing else", {}},
        {"W2 TextBox focused, another window activated, the probe window activated again",
         {{0, [] { probe->field.Focus(xaml::FocusState::Programmatic); }},
          {8, [] { ::SetForegroundWindow(probe->other); }},
          {18, [] { ::SetForegroundWindow(probe->hwnd); }}}},
        {"W3 Button focused, window minimized and restored",
         {{0, [] { probe->button.Focus(xaml::FocusState::Programmatic); }},
          {4, [] { ::ShowWindow(probe->hwnd, SW_MINIMIZE); }},
          {14, [] { ::ShowWindow(probe->hwnd, SW_RESTORE); }}}},
        {"W4 Tab on the last element (Button)",
         {{0, [] { probe->button.Focus(xaml::FocusState::Keyboard); }},
          {8, [] { key(VK_TAB, true); }},
          {10, [] { key(VK_TAB, false); }}}},
        {"W5 Shift+Tab on the first element (TextBox)",
         {{0, [] { probe->field.Focus(xaml::FocusState::Keyboard); }},
          {6, [] { key(VK_SHIFT, true); }},
          {8, [] { key(VK_TAB, true); }},
          {10, [] { key(VK_TAB, false); }},
          {12, [] { key(VK_SHIFT, false); }}}},
    };
}

void tick() {
    int const s = probe->step++;
    std::size_t const index = static_cast<std::size_t>(s / phaseTicks);
    int const offset = s % phaseTicks;
    if (index >= probe->phases.size()) {
        say("done");
        ::KillTimer(probe->hwnd, 1);
        ::PostMessageW(probe->hwnd, WM_CLOSE, 0, 0);
        return;
    }
    Phase const& phase = probe->phases[index];
    if (offset == 0) {
        say("-- %s", phase.name);
        probe->sensor.ChangeView(nullptr, nullptr, 1.0f, true);
    }
    for (Step const& step : phase.steps) {
        if (step.offset == offset) step.run();
    }
    // Ctrl держится весь нажим, как его держит человек.
    if (offset == 28) describeFocus();
    if (offset == 30) key(VK_CONTROL, true);
    if (offset == 33) key(static_cast<WORD>(plusKey), true);
    if (offset == 35) key(static_cast<WORD>(plusKey), false);
    if (offset == 40) key(VK_CONTROL, false);
    if (offset == 75) {
        std::string order;
        for (auto const& what : probe->seen) order += (order.empty() ? "" : " -> ") + what;
        say("== phase %zu: %s; sensor zoom %.3f", index + 1, order.empty() ? "NOBODY" : order.c_str(),
            probe->sensor.ZoomFactor());
        probe->seen.clear();
    }
}

// ---- окно и остров ----

void onIslandLoaded() {
    listChildWindows();
    if (probe->islandFocusPending) {
        bool const active = ::GetActiveWindow() == probe->hwnd;
        say("  island loaded with focus pending; window active: %d", active ? 1 : 0);
        if (active) setFocusToContentIsland();
        probe->islandFocusPending = false;
    }
    try {
        probe->islandKeyboard =
            winrt::Microsoft::UI::Input::InputKeyboardSource::GetForIsland(probe->root.XamlRoot().ContentIsland());
    } catch (winrt::hresult_error const& error) {
        say("InputKeyboardSource::GetForIsland failed: %08x", static_cast<unsigned>(error.code().value));
        return;
    }
    say("island InputKeyboardSource: yes");
    probe->islandKeyboard.KeyDown([](auto&&, winrt::Microsoft::UI::Input::KeyEventArgs const& args) {
        if (args.VirtualKey() == static_cast<VirtualKey>(plusKey)) seen("island.KeyDown" + handledNote(args.Handled()));
    });
}

void buildContent() {
    probe->field = controls::TextBox{};
    probe->field.Name(L"field");
    probe->field.Width(240);
    probe->button = controls::Button{};
    probe->button.Name(L"button");
    probe->button.Content(winrt::box_value(L"Кнопка"));

    controls::StackPanel top;
    top.Spacing(12);
    top.Margin(xaml::ThicknessHelper::FromUniformLength(24));
    top.HorizontalAlignment(xaml::HorizontalAlignment::Left);
    top.VerticalAlignment(xaml::VerticalAlignment::Top);
    top.Children().Append(probe->field);
    top.Children().Append(probe->button);

    // Всплывающее: своё содержимое со своим PreviewKeyDown.
    probe->popupField = controls::TextBox{};
    probe->popupField.Name(L"popupField");
    probe->popupField.Width(200);
    controls::StackPanel popup;
    popup.Children().Append(probe->popupField);
    popup.PreviewKeyDown([](auto&&, xaml::Input::KeyRoutedEventArgs const& args) {
        if (isPlus(args)) seen("popup.PreviewKeyDown" + handledNote(args.Handled()));
    });
    probe->flyout = controls::Flyout{};
    probe->flyout.Content(popup);

    // Корень -- как в CompositionWindow: Grid во всё окно с прозрачным фоном,
    // чтобы щелчок по пустому месту попадал в него.
    probe->root = controls::Grid{};
    probe->root.Background(xaml::Media::SolidColorBrush{mu::Colors::Transparent()});
    probe->root.Children().Append(top);
    probe->root.Loaded([](auto&&, auto&&) { onIslandLoaded(); });
    probe->root.AddHandler(xaml::UIElement::PointerPressedEvent(),
                           winrt::box_value(xaml::Input::PointerEventHandler(
                               [](auto&&, xaml::Input::PointerRoutedEventArgs const& args) {
                                   say("  root PointerPressed%s", handledNote(args.Handled()).c_str());
                               })),
                           true);
    probe->root.PreviewKeyDown([](auto&&, xaml::Input::KeyRoutedEventArgs const& args) {
        if (isPlus(args)) seen("root.PreviewKeyDown" + handledNote(args.Handled()));
    });
    probe->root.AddHandler(xaml::UIElement::KeyDownEvent(),
                           winrt::box_value(xaml::Input::KeyEventHandler(
                               [](auto&&, xaml::Input::KeyRoutedEventArgs const& args) {
                                   if (isPlus(args)) seen("root.KeyDown(all)" + handledNote(args.Handled()));
                               })),
                           true);
    xaml::Input::KeyboardAccelerator accelerator;
    accelerator.Key(static_cast<VirtualKey>(plusKey));
    accelerator.Modifiers(VirtualKeyModifiers::Control);
    accelerator.Invoked([](auto&&, xaml::Input::KeyboardAcceleratorInvokedEventArgs const& args) {
        seen("root.KeyboardAccelerator" + handledNote(args.Handled()));
    });
    probe->root.KeyboardAccelerators().Append(accelerator);

    // Датчик жеста над корнем -- верхний элемент острова, как в CompositionWindow.
    probe->sensor = controls::ScrollViewer{};
    probe->sensor.ZoomMode(controls::ZoomMode::Enabled);
    probe->sensor.HorizontalScrollMode(controls::ScrollMode::Disabled);
    probe->sensor.VerticalScrollMode(controls::ScrollMode::Disabled);
    probe->sensor.HorizontalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
    probe->sensor.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
    probe->sensor.Content(probe->root);
    probe->sensor.PreviewKeyDown([](auto&&, xaml::Input::KeyRoutedEventArgs const& args) {
        if (isPlus(args)) seen("sensor.PreviewKeyDown" + handledNote(args.Handled()));
    });
    probe->sensor.AddHandler(xaml::UIElement::KeyDownEvent(),
                             winrt::box_value(xaml::Input::KeyEventHandler(
                                 [](auto&&, xaml::Input::KeyRoutedEventArgs const& args) {
                                     if (isPlus(args)) seen("sensor.KeyDown(all)" + handledNote(args.Handled()));
                                 })),
                             true);
}

void resize() {
    RECT client{};
    ::GetClientRect(probe->hwnd, &client);
    if (probe->xamlSource) probe->xamlSource.SiteBridge().MoveAndResize({0, 0, client.right, client.bottom});
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (!probe || hwnd != probe->hwnd) return ::DefWindowProcW(hwnd, message, wparam, lparam);
    switch (message) {
        case WM_SIZE:
            resize();
            break;
        case WM_TIMER:
            tick();
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wparam == plusKey) seen("WndProc WM_KEYDOWN");
            break;
        case WM_ACTIVATE:
            // Как DesktopWindowImpl::OnMessage: WM_ACTIVATE до DefWindowProc не
            // доходит -- тот поставил бы фокус на само окно.
            if (probe->windowLike) {
                onActivate(wparam);
                return 0;
            }
            break;
        case WM_SETFOCUS:
            say("  WndProc WM_SETFOCUS (from %s)", className(reinterpret_cast<HWND>(wparam)).c_str());
            if (probe->restoreFocusOnActivation && probe->xamlSource) {
                auto const result = probe->xamlSource.NavigateFocus(xaml::Hosting::XamlSourceFocusNavigationRequest{
                    xaml::Hosting::XamlSourceFocusNavigationReason::Restore});
                say("  NavigateFocus(Restore): WasFocusMoved=%d", result.WasFocusMoved() ? 1 : 0);
            }
            break;
        case WM_KILLFOCUS:
            say("  WndProc WM_KILLFOCUS (to %s)", className(reinterpret_cast<HWND>(wparam)).c_str());
            break;
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
    logPath += L"keyboard-zoom-probe.log";
    logFile = ::_wfopen(logPath.c_str(), L"w");
    startTick = ::GetTickCount64();

    probe = new Probe{};
    std::wstring_view const commandLine{::GetCommandLineW()};
    bool const infoFlyoutMode = commandLine.find(L"--info-flyout") != std::wstring_view::npos;
    probe->windowLike = infoFlyoutMode || commandLine.find(L"--window-like") != std::wstring_view::npos;
    probe->phases = infoFlyoutMode ? infoFlyoutPhases() : probe->windowLike ? windowLikePhases() : phases();
    say("mode: %s", probe->windowLike ? "window-like (focus as Microsoft.UI.Xaml.Window)" : "plain");

    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = ::GetModuleHandleW(nullptr);
    wc.lpszClassName = L"wxl.KeyboardZoomProbe";
    wc.hCursor = ::LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    ::RegisterClassExW(&wc);

    probe->hwnd = ::CreateWindowExW(0, wc.lpszClassName, L"Проба клавиш масштаба", WS_OVERLAPPEDWINDOW, 200, 120, 900,
                                    640, nullptr, nullptr, wc.hInstance, nullptr);
    probe->other = ::CreateWindowExW(0, wc.lpszClassName, L"Проба клавиш масштаба: другое окно",
                                     WS_OVERLAPPEDWINDOW, 1150, 120, 400, 300, nullptr, nullptr, wc.hInstance, nullptr);

    probe->getMessageHook = ::SetWindowsHookExW(WH_GETMESSAGE, onGetMessage, nullptr, ::GetCurrentThreadId());
    probe->keyboardHook = ::SetWindowsHookExW(WH_KEYBOARD, onKeyboard, nullptr, ::GetCurrentThreadId());
    say("hooks: WH_GETMESSAGE %s, WH_KEYBOARD %s", probe->getMessageHook ? "yes" : "NO",
        probe->keyboardHook ? "yes" : "NO");

    buildContent();
    probe->xamlSource = xaml::Hosting::DesktopWindowXamlSource{};
    probe->xamlSource.Initialize(mu::WindowId{static_cast<uint64_t>(reinterpret_cast<uintptr_t>(probe->hwnd))});
    probe->xamlSource.Content(probe->sensor);
    say("XamlRoot right after Content(): %s", probe->root.XamlRoot() ? "yes" : "no");
    if (probe->windowLike) {
        probe->xamlSource.TakeFocusRequested(
            [](auto&&, xaml::Hosting::DesktopWindowXamlSourceTakeFocusRequestedEventArgs const& args) {
                onTakeFocusRequested(args);
            });
    }
    resize();
    probe->xamlSource.SiteBridge().Show();

    ::ShowWindow(probe->other, SW_SHOWNOACTIVATE);
    ::ShowWindow(probe->hwnd, SW_SHOW);
    ::UpdateWindow(probe->hwnd);
    ::SetTimer(probe->hwnd, 1, 16, nullptr);
    say("shown");

    return [](wxl::TeardownReason) {
        if (probe->getMessageHook) ::UnhookWindowsHookEx(probe->getMessageHook);
        if (probe->keyboardHook) ::UnhookWindowsHookEx(probe->keyboardHook);
        say("teardown");
        if (logFile) std::fclose(logFile);
    };
}
