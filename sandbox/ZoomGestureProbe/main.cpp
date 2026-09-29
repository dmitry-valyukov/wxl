// Проба масштаба жестом и колесом у корня острова XAML.
//
// Два вопроса перед тем, как ZoomEffect возьмёт на себя жест масштабирования:
//
//   1. Доходит ли жест масштабирования (два пальца) до ScrollViewer-датчика у
//      корня острова, если под пальцами -- список в своём ScrollViewer с
//      выключенным масштабом? Так ловил жест ZoomView Беседки, но у каждого
//      списка свой; здесь датчик один на всё окно, и жест должен дойти до него
//      по цепочке масштабирования (IsZoomChainingEnabled).
//   2. Можно ли погасить Ctrl+колесо до XAML -- хуком WH_GETMESSAGE на потоке
//      интерфейса, -- чтобы список его не получал и не прокручивался, а
//      масштаб оставался за эффектом? Средствами XAML нельзя: колесо
//      всплывает от элемента под указателем к корню, и туннельного события у
//      указателя нет.
//
// Проба прогоняет сценарий сама и закрывается; всё пишет в
// zoom-gesture-probe.log рядом с исполняемым файлом:
//
//   А. касание двумя пальцами (InjectSyntheticPointerInput) над списком --
//      пальцы расходятся;
//   Б. Ctrl+колесо (SendInput) над списком, без хука;
//   В. то же с хуком, гасящим Ctrl+колесо;
//   Г. обычное колесо с тем же хуком -- оно должно дойти до списка;
//   Д. Ctrl+колесо, которое гасит источник ввода острова (InputPointerSource),
//      -- получает ли он колесо раньше XAML;
//   Е. Ctrl+колесо, помеченное обработанным на элементе сразу под датчиком,
//      -- перестаёт ли датчик масштабировать, доходит ли колесо до корня.
//
// SendInput двигает настоящий указатель и нажимает настоящий Ctrl: на время
// пробы мышь и клавиатура -- её.

#include "platform.h"

#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>

#include <cstdarg>
#include <cstdio>
#include <string>

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

struct Probe {
    HWND hwnd = nullptr;
    xaml::Hosting::DesktopWindowXamlSource xamlSource{nullptr};
    controls::Grid root{nullptr};
    controls::ScrollViewer sensor{nullptr};
    controls::Grid underSensor{nullptr};
    controls::ScrollViewer list{nullptr};
    winrt::Microsoft::UI::Input::InputPointerSource islandPointer{nullptr};

    bool islandSwallows = false;  // этап Д
    bool underSwallows = false;   // этап Е
    int islandWheel = 0;          // колесо у источника ввода острова

    HSYNTHETICPOINTERDEVICE touch = nullptr;
    HHOOK hook = nullptr;
    int wheelSeen = 0;       // WM_MOUSEWHEEL, дошедшие до хука
    int wheelSwallowed = 0;  // из них погашены
    int rootWheel = 0;       // PointerWheelChanged у корня (handledEventsToo)

    int step = 0;
    POINT center{};
    double listOffsetBefore = 0;
};

Probe* probe = nullptr;

// ---- хук: гасит Ctrl+колесо до того, как его разберёт XAML ----

LRESULT CALLBACK getMessageHook(int code, WPARAM wparam, LPARAM lparam) {
    if (code == HC_ACTION && wparam == PM_REMOVE) {
        auto* message = reinterpret_cast<MSG*>(lparam);
        if (message->message == WM_MOUSEWHEEL || message->message == WM_POINTERWHEEL) {
            ++probe->wheelSeen;
            bool const ctrl = message->message == WM_MOUSEWHEEL
                                  ? (GET_KEYSTATE_WPARAM(message->wParam) & MK_CONTROL) != 0
                                  : (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
            say("hook: %s ctrl=%d hwnd=%p", message->message == WM_MOUSEWHEEL ? "WM_MOUSEWHEEL" : "WM_POINTERWHEEL",
                ctrl ? 1 : 0, static_cast<void*>(message->hwnd));
            if (ctrl) {
                ++probe->wheelSwallowed;
                message->message = WM_NULL;
            }
        }
    }
    return ::CallNextHookEx(nullptr, code, wparam, lparam);
}

void setHook(bool on) {
    if (on && !probe->hook) {
        probe->hook = ::SetWindowsHookExW(WH_GETMESSAGE, getMessageHook, nullptr, ::GetCurrentThreadId());
        say("hook installed: %s", probe->hook ? "yes" : "NO");
    } else if (!on && probe->hook) {
        ::UnhookWindowsHookEx(probe->hook);
        probe->hook = nullptr;
        say("hook removed");
    }
}

// ---- ввод ----

void touchFrame(int apart, POINTER_FLAGS flags) {
    POINTER_TYPE_INFO contacts[2]{};
    for (int i = 0; i < 2; ++i) {
        auto& info = contacts[i];
        info.type = PT_TOUCH;
        auto& t = info.touchInfo;
        t.pointerInfo.pointerType = PT_TOUCH;
        t.pointerInfo.pointerId = static_cast<UINT32>(i);
        t.pointerInfo.ptPixelLocation = {probe->center.x + (i == 0 ? -apart / 2 : apart / 2), probe->center.y};
        t.pointerInfo.pointerFlags = flags;
        t.touchFlags = TOUCH_FLAG_NONE;
        t.touchMask = TOUCH_MASK_CONTACTAREA | TOUCH_MASK_ORIENTATION | TOUCH_MASK_PRESSURE;
        t.rcContact = {t.pointerInfo.ptPixelLocation.x - 2, t.pointerInfo.ptPixelLocation.y - 2,
                       t.pointerInfo.ptPixelLocation.x + 2, t.pointerInfo.ptPixelLocation.y + 2};
        t.orientation = 90;
        t.pressure = 32000;
    }
    if (!::InjectSyntheticPointerInput(probe->touch, contacts, 2)) {
        say("InjectSyntheticPointerInput failed: %lu", ::GetLastError());
    }
}

// Ctrl держится весь этап, как его держит человек: нажатый и отпущенный в
// одной пачке с колесом, он отпущен раньше, чем XAML разберёт колесо, и
// колесо приходит без Ctrl (первый прогон пробы).
void ctrl(bool down) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = VK_CONTROL;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    ::SendInput(1, &input, sizeof(INPUT));
    say("Ctrl %s", down ? "down" : "up");
}

void wheel(int delta) {
    ::SetCursorPos(probe->center.x, probe->center.y);
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_WHEEL;
    input.mi.mouseData = static_cast<DWORD>(delta);
    UINT const sent = ::SendInput(1, &input, sizeof(INPUT));
    say("SendInput wheel %+d: %u", delta, sent);
}

void report(const char* phase) {
    say("== %s: sensor zoom=%.3f list offset %.1f -> %.1f; hook saw %d wheel, swallowed %d; island source %d; root "
        "PointerWheelChanged %d",
        phase, probe->sensor.ZoomFactor(), probe->listOffsetBefore, probe->list.VerticalOffset(), probe->wheelSeen,
        probe->wheelSwallowed, probe->islandWheel, probe->rootWheel);
    probe->wheelSeen = probe->wheelSwallowed = probe->rootWheel = probe->islandWheel = 0;
}

void beginPhase(const char* phase) {
    probe->sensor.ChangeView(nullptr, nullptr, 1.0f, true);
    probe->list.ChangeView(nullptr, 2000.0, nullptr, true);
    probe->listOffsetBefore = 2000.0;
    say("-- %s", phase);
}

// Сценарий по тикам таймера в 16 мс: этап, затем секунда на успокоение.
void tick() {
    int const s = probe->step++;
    constexpr int pinchFrames = 30;
    constexpr int settle = 60;

    // А: пальцы расходятся со 100 до 400 пикселей.
    constexpr int a = 20;
    if (s == a) {
        ::SetForegroundWindow(probe->hwnd);
        RECT client{};
        ::GetClientRect(probe->hwnd, &client);
        probe->center = {client.right / 2, client.bottom / 2};
        ::ClientToScreen(probe->hwnd, &probe->center);
        say("center %ld,%ld", probe->center.x, probe->center.y);
        beginPhase("A: two-finger spread over the list");
        touchFrame(100, POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT);
    } else if (s > a && s <= a + pinchFrames) {
        touchFrame(100 + 10 * (s - a), POINTER_FLAG_UPDATE | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT);
    } else if (s == a + pinchFrames + 1) {
        touchFrame(100 + 10 * pinchFrames, POINTER_FLAG_UP);
    } else if (s == a + pinchFrames + 1 + settle) {
        report("A");
    }

    // Б: Ctrl+колесо без хука.
    constexpr int b = a + pinchFrames + 2 + settle;
    if (s == b) {
        beginPhase("B: Ctrl+wheel, no hook");
        ctrl(true);
    } else if (s == b + 5 || s == b + 10 || s == b + 15) {
        wheel(WHEEL_DELTA);
    } else if (s == b + 15 + settle) {
        ctrl(false);
        report("B");
    }

    // В: Ctrl+колесо с хуком.
    constexpr int c = b + 16 + settle;
    if (s == c) {
        beginPhase("C: Ctrl+wheel, hook swallows it");
        ctrl(true);
        setHook(true);
    } else if (s == c + 5 || s == c + 10 || s == c + 15) {
        wheel(WHEEL_DELTA);
    } else if (s == c + 15 + settle) {
        ctrl(false);
        report("C");
        setHook(false);
    }

    // Г: обычное колесо с тем же хуком.
    constexpr int d = c + 16 + settle;
    if (s == d) {
        beginPhase("D: plain wheel, hook installed");
        setHook(true);
    } else if (s == d + 5 || s == d + 10 || s == d + 15) {
        wheel(-WHEEL_DELTA);
    } else if (s == d + 15 + settle) {
        report("D");
        setHook(false);
    }

    // Д: Ctrl+колесо гасит источник ввода острова.
    constexpr int e = d + 16 + settle;
    if (s == e) {
        beginPhase("E: Ctrl+wheel, island InputPointerSource marks it handled");
        ctrl(true);
        probe->islandSwallows = true;
    } else if (s == e + 5 || s == e + 10 || s == e + 15) {
        wheel(WHEEL_DELTA);
    } else if (s == e + 15 + settle) {
        ctrl(false);
        report("E");
        probe->islandSwallows = false;
    }

    // Е: Ctrl+колесо помечено обработанным сразу под датчиком.
    constexpr int f = e + 16 + settle;
    if (s == f) {
        beginPhase("F: Ctrl+wheel, handled on the element under the sensor");
        ctrl(true);
        probe->underSwallows = true;
    } else if (s == f + 5 || s == f + 10 || s == f + 15) {
        wheel(WHEEL_DELTA);
    } else if (s == f + 15 + settle) {
        ctrl(false);
        report("F");
        probe->underSwallows = false;
        say("done");
        ::KillTimer(probe->hwnd, 1);
        ::PostMessageW(probe->hwnd, WM_CLOSE, 0, 0);
    }
}

bool ctrlIn(winrt::Windows::System::VirtualKeyModifiers modifiers) {
    return (static_cast<uint32_t>(modifiers) & static_cast<uint32_t>(winrt::Windows::System::VirtualKeyModifiers::Control)) != 0;
}

// Источник ввода острова XAML: остров берётся у XamlRoot, когда он появился.
void subscribeIsland() {
    auto const island = probe->root.XamlRoot().ContentIsland();
    try {
        probe->islandPointer = winrt::Microsoft::UI::Input::InputPointerSource::GetForIsland(island);
    } catch (winrt::hresult_error const& error) {
        say("InputPointerSource::GetForIsland failed: %08x", static_cast<unsigned>(error.code().value));
        return;
    }
    say("island InputPointerSource: yes");
    probe->islandPointer.PointerWheelChanged(
        [](auto&&, winrt::Microsoft::UI::Input::PointerEventArgs const& args) {
            ++probe->islandWheel;
            bool const ctrl = ctrlIn(args.KeyModifiers());
            say("island PointerWheelChanged ctrl=%d handled=%d", ctrl ? 1 : 0, args.Handled() ? 1 : 0);
            if (probe->islandSwallows && ctrl) args.Handled(true);
        });
}

// ---- окно и остров ----

void buildContent() {
    controls::StackPanel lines;
    for (int i = 1; i <= 300; ++i) {
        controls::TextBlock line;
        line.Text(winrt::hstring{L"Строка " + std::to_wstring(i)});
        line.FontSize(18);
        lines.Children().Append(line);
    }

    probe->list = controls::ScrollViewer{};
    probe->list.Content(lines);
    probe->list.ViewChanged([](auto&&, controls::ScrollViewerViewChangedEventArgs const& args) {
        if (!args.IsIntermediate()) say("list ViewChanged offset=%.1f", probe->list.VerticalOffset());
    });

    // Датчик у корня: масштаб включён, прокрутки нет -- содержимое всегда во
    // всю ширину и высоту окна, как без него.
    probe->sensor = controls::ScrollViewer{};
    probe->sensor.ZoomMode(controls::ZoomMode::Enabled);
    probe->sensor.MinZoomFactor(0.5f);
    probe->sensor.MaxZoomFactor(3.0f);
    probe->sensor.HorizontalScrollMode(controls::ScrollMode::Disabled);
    probe->sensor.VerticalScrollMode(controls::ScrollMode::Disabled);
    probe->sensor.HorizontalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
    probe->sensor.VerticalScrollBarVisibility(controls::ScrollBarVisibility::Disabled);
    // Элемент сразу под датчиком: на этапе Е помечает Ctrl+колесо обработанным,
    // пока оно не всплыло до датчика.
    probe->underSensor = controls::Grid{};
    probe->underSensor.Children().Append(probe->list);
    probe->underSensor.PointerWheelChanged([](auto&&, xaml::Input::PointerRoutedEventArgs const& args) {
        if (probe->underSwallows && ctrlIn(args.KeyModifiers())) {
            args.Handled(true);
            say("under-sensor marks Ctrl+wheel handled");
        }
    });
    probe->sensor.Content(probe->underSensor);
    probe->sensor.ViewChanged([](auto&&, controls::ScrollViewerViewChangedEventArgs const& args) {
        say("sensor ViewChanged zoom=%.3f intermediate=%d", probe->sensor.ZoomFactor(), args.IsIntermediate() ? 1 : 0);
    });

    probe->root = controls::Grid{};
    probe->root.Children().Append(probe->sensor);
    probe->root.Loaded([](auto&&, auto&&) { subscribeIsland(); });
    probe->root.AddHandler(xaml::UIElement::PointerWheelChangedEvent(),
                           winrt::box_value(xaml::Input::PointerEventHandler(
                               [](auto&&, xaml::Input::PointerRoutedEventArgs const& args) {
                                   ++probe->rootWheel;
                                   say("root PointerWheelChanged handled=%d", args.Handled() ? 1 : 0);
                               })),
                           true);
}

void resize() {
    RECT client{};
    ::GetClientRect(probe->hwnd, &client);
    if (probe->xamlSource) probe->xamlSource.SiteBridge().MoveAndResize({0, 0, client.right, client.bottom});
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (!probe) return ::DefWindowProcW(hwnd, message, wparam, lparam);
    switch (message) {
        case WM_SIZE:
            resize();
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
    logPath += L"zoom-gesture-probe.log";
    logFile = ::_wfopen(logPath.c_str(), L"w");
    startTick = ::GetTickCount64();

    probe = new Probe{};

    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.lpfnWndProc = windowProc;
    wc.hInstance = ::GetModuleHandleW(nullptr);
    wc.lpszClassName = L"wxl.ZoomGestureProbe";
    wc.hCursor = ::LoadCursorW(nullptr, reinterpret_cast<LPCWSTR>(IDC_ARROW));
    ::RegisterClassExW(&wc);

    probe->hwnd = ::CreateWindowExW(0, wc.lpszClassName, L"Проба масштаба жестом", WS_OVERLAPPEDWINDOW, 200, 120,
                                    900, 640, nullptr, nullptr, wc.hInstance, nullptr);

    probe->touch = ::CreateSyntheticPointerDevice(PT_TOUCH, 2, POINTER_FEEDBACK_DEFAULT);
    say("synthetic touch device: %s", probe->touch ? "yes" : "NO");

    buildContent();
    probe->xamlSource = xaml::Hosting::DesktopWindowXamlSource{};
    probe->xamlSource.Initialize(mu::WindowId{static_cast<uint64_t>(reinterpret_cast<uintptr_t>(probe->hwnd))});
    probe->xamlSource.Content(probe->root);
    resize();
    probe->xamlSource.SiteBridge().Show();

    ::ShowWindow(probe->hwnd, SW_SHOW);
    ::UpdateWindow(probe->hwnd);
    ::SetTimer(probe->hwnd, 1, 16, nullptr);
    say("shown");

    return [](wxl::TeardownReason) {
        setHook(false);
        if (probe->touch) ::DestroySyntheticPointerDevice(probe->touch);
        say("teardown");
        if (logFile) std::fclose(logFile);
    };
}
