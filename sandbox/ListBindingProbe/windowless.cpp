// ListBindingProbe, вопрос (е): поднимается ли XAML в потоке без Application::Start и без показанного окна -- так, как
// его поднял бы тест ctest (wxl.ui.list-binding), -- и строит ли там список свои элементы.
//
// Своя точка входа main, без wxl.ui: wxl.ui уводит вход на свой wWinMain, который сам зовёт Application::Start (как у
// sandbox/CppWinRTPanel). Порядок -- как у примера островов Windows App SDK (Samples/Islands): DispatcherQueue на
// потоке, объект Application со своим поставщиком метаданных, WindowsXamlManager::InitializeForCurrentThread, ресурсы
// контролов. Потом одна и та же пара (ListView с шаблоном, как у wxl, и ItemsRepeater с фабрикой) -- в трёх местах:
//
//   detached  ни в каком дереве: Measure/Arrange/UpdateLayout руками;
//   island    XamlIsland без места (ChildSiteLink) и без окна;
//   hidden    DesktopWindowXamlSource на окне, которое ни разу не показано.
//
// Ответ -- строка "(e) ..." в stdout; код выхода -- 0, если без показанного окна список строит элементы, иначе 1.
// Запускает её ListBindingProbe и вставляет вывод в свой; запускать можно и саму.

#include "platform.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.h>
#include <winrt/Microsoft.UI.Content.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Hosting.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

#include <impl/bootstrap.h>
#include <impl/hresult.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <string>

#include "common.h"

using namespace probe;

namespace {

std::string vformat(char const* format, va_list args) {
    va_list copy;
    va_copy(copy, args);
    int const size = std::vsnprintf(nullptr, 0, format, copy);
    va_end(copy);
    if (size <= 0) return {};
    std::string text(static_cast<size_t>(size), '\0');
    std::vsnprintf(text.data(), text.size() + 1, format, args);
    return text;
}

std::string strf(char const* format, ...) {
    va_list args;
    va_start(args, format);
    std::string text = vformat(format, args);
    va_end(args);
    return text;
}

void say(std::string const& text) {
    std::fputs(text.c_str(), stdout);
    std::fputc('\n', stdout);
    std::fflush(stdout);
}

std::string describe(winrt::hresult_error const& error) {
    return strf("0x%08X %s", static_cast<uint32_t>(error.code().value), winrt::to_string(error.message()).c_str());
}

constexpr char const* question = "can XAML run on a thread without Application::Start and without a shown window, for a ctest test";

// Объект Application без Start: через него XAML находит стили контролов и метаданные типов, которые шаблон контрола
// называет (без них ItemsView и ItemContainer не строят шаблон).
struct ProbeApp : xaml::ApplicationT<ProbeApp, xaml::Markup::IXamlMetadataProvider> {
    void OnLaunched(xaml::LaunchActivatedEventArgs const&) {
        launched = true;
        mergeResources();
    }

    void mergeResources() {
        if (merged) return;
        Resources().MergedDictionaries().Append(controls::XamlControlsResources {});
        merged = true;
    }

    auto GetXamlType(winrt::Windows::UI::Xaml::Interop::TypeName const& type) const { return provider_.GetXamlType(type); }
    auto GetXamlType(winrt::hstring const& name) const { return provider_.GetXamlType(name); }
    auto GetXmlnsDefinitions() const { return provider_.GetXmlnsDefinitions(); }

    bool launched = false;
    bool merged = false;

private:
    xaml::XamlTypeInfo::XamlControlsXamlMetaDataProvider provider_;
};

// Сообщения потока -- столько, сколько сказано: фазы контейнеров ListView XAML исполняет на своих тиках.
void pump(UINT milliseconds) {
    UINT_PTR const timer = ::SetTimer(nullptr, 0, milliseconds, nullptr);
    MSG message {};
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (message.message == WM_TIMER && message.hwnd == nullptr && message.wParam == timer) break;
        ::TranslateMessage(&message);
        ::DispatchMessageW(&message);
    }
    ::KillTimer(nullptr, timer);
}

wf::IInspectable numbers(int count) {
    auto items = winrt::single_threaded_vector<wf::IInspectable>();
    for (int64_t i = 0; i < count; ++i) items.Append(winrt::box_value(i));
    return items;
}

// Пара списков в одном месте. Не разрушается: элементы считают себя в её счётчиках, а XAML отпускает их, когда хочет.
struct Host {
    Census listCensus;
    Census repeaterCensus;
    TemplateStats stats;
    winrt::com_ptr<Factory> factory;
    controls::ListView list {nullptr};
    controls::ItemsRepeater repeater {nullptr};
    controls::StackPanel root {nullptr};
};

Host* makeHost() {
    auto* host = new Host {};
    host->list = controls::ListView {};
    host->list.Width(200);
    host->list.Height(300);
    hookTemplate(host->list, &host->listCensus, &host->stats);
    host->list.ItemsSource(numbers(40));

    host->factory = winrt::make_self<Factory>(&host->repeaterCensus, Recycle::RemoveFromParent, false);
    host->repeater = controls::ItemsRepeater {};
    host->repeater.ItemTemplate(host->factory.as<xaml::IElementFactory>());
    host->repeater.ItemsSource(numbers(40));
    controls::ScrollViewer viewer;
    viewer.Width(200);
    viewer.Height(300);
    viewer.Content(host->repeater);

    host->root = controls::StackPanel {};
    host->root.Orientation(controls::Orientation::Horizontal);
    host->root.Width(420);
    host->root.Height(300);
    host->root.Children().Append(host->list);
    host->root.Children().Append(viewer);
    return host;
}

struct Seen {
    bool tried = false;
    std::string error;
    int phase0 = 0;
    int phase1 = 0;
    bool firstContainer = false;
    int repeaterBuilt = 0;

    bool works() const { return error.empty() && phase1 > 0 && repeaterBuilt > 0; }

    std::string text() const {
        if (!tried) return "not tried";
        if (!error.empty()) return "failed: " + error;
        return strf("ListView phase 0 x%d, phase 1 x%d, first container %s; ItemsRepeater built %d", phase0, phase1,
                    firstContainer ? "yes" : "no", repeaterBuilt);
    }
};

void look(Host const& host, Seen& seen) {
    seen.phase0 = host.stats.phase0;
    seen.phase1 = host.stats.phase1;
    seen.firstContainer = static_cast<bool>(host.list.ContainerFromIndex(0));
    seen.repeaterBuilt = host.repeaterCensus.built;
}

}  // namespace

int main() {
    winrt::init_apartment(winrt::apartment_type::single_threaded);
    try {
        wxl::impl::ensure_windows_app_runtime_initialized();
    } catch (wxl::hresult_error const& error) {
        say(strf("(e) %s -- the Windows App Runtime did not come up: 0x%08X -- not answered [SURPRISE]", question,
                 static_cast<uint32_t>(error.code())));
        return 1;
    }

    auto const controller = winrt::Microsoft::UI::Dispatching::DispatcherQueueController::CreateOnCurrentThread();

    winrt::com_ptr<ProbeApp> application;
    xaml::Hosting::WindowsXamlManager manager {nullptr};
    std::string init;
    try {
        application = winrt::make_self<ProbeApp>();
        manager = xaml::Hosting::WindowsXamlManager::InitializeForCurrentThread();
        bool const launched = application->launched;
        application->mergeResources();
        init = strf("Application made without Start, WindowsXamlManager up, OnLaunched %s",
                    launched ? "called by the framework" : "not called (resources merged by hand)");
    } catch (winrt::hresult_error const& error) {
        say(strf("(e) %s -- XAML did not come up on the thread: %s -- no ctest without Application::Start [SURPRISE]",
                 question, describe(error).c_str()));
        return 1;
    }
    say(init);

    // detached
    Seen detached;
    detached.tried = true;
    try {
        Host* const host = makeHost();
        host->root.Measure(wf::Size {420, 300});
        host->root.Arrange(wf::Rect {0, 0, 420, 300});
        host->root.UpdateLayout();
        pump(600);
        host->root.UpdateLayout();
        pump(300);
        look(*host, detached);
    } catch (winrt::hresult_error const& error) {
        detached.error = describe(error);
    }
    say("detached element: " + detached.text());

    // island
    Seen island;
    island.tried = true;
    xaml::XamlIsland xamlIsland {nullptr};
    try {
        Host* const host = makeHost();
        xamlIsland = xaml::XamlIsland {};
        xamlIsland.Content(host->root);
        host->root.UpdateLayout();
        pump(600);
        host->root.UpdateLayout();
        pump(300);
        look(*host, island);
    } catch (winrt::hresult_error const& error) {
        island.error = describe(error);
    }
    say("XamlIsland without a site or a window: " + island.text());

    // hidden
    Seen hidden;
    hidden.tried = true;
    HWND hwnd = nullptr;
    xaml::Hosting::DesktopWindowXamlSource source {nullptr};
    try {
        WNDCLASSEXW windowClass {};
        windowClass.cbSize = sizeof windowClass;
        windowClass.lpfnWndProc = ::DefWindowProcW;
        windowClass.hInstance = ::GetModuleHandleW(nullptr);
        windowClass.lpszClassName = L"wxl.ListBindingWindowless";
        ::RegisterClassExW(&windowClass);
        hwnd = ::CreateWindowExW(0, windowClass.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 500, 400, nullptr, nullptr,
                                 windowClass.hInstance, nullptr);
        Host* const host = makeHost();
        source = xaml::Hosting::DesktopWindowXamlSource {};
        source.Initialize(winrt::Microsoft::UI::WindowId {static_cast<uint64_t>(reinterpret_cast<uintptr_t>(hwnd))});
        source.Content(host->root);
        source.SiteBridge().MoveAndResize(winrt::Windows::Graphics::RectInt32 {0, 0, 500, 400});
        host->root.UpdateLayout();
        pump(600);
        host->root.UpdateLayout();
        pump(300);
        look(*host, hidden);
    } catch (winrt::hresult_error const& error) {
        hidden.error = describe(error);
    }
    say("DesktopWindowXamlSource on a window never shown: " + hidden.text());

    std::string conclusion;
    if (island.works()) {
        conclusion = "yes: a ctest program brings XAML up without Start and hosts the list in a XamlIsland with no window";
    } else if (detached.works()) {
        conclusion = "yes: a ctest program brings XAML up without Start; the list works detached, measured by hand";
    } else if (hidden.works()) {
        conclusion = "only with a window, but it may stay hidden: DesktopWindowXamlSource on an unshown HWND";
    } else {
        conclusion = "no: none of the three realizes the list without a shown window";
    }
    bool const expected = island.works() || detached.works();
    say(strf("(e) %s -- %s; detached: %s; XamlIsland: %s; hidden window: %s -- %s %s", question, init.c_str(),
             detached.text().c_str(), island.text().c_str(), hidden.text().c_str(), conclusion.c_str(),
             expected ? "[as expected]" : "[SURPRISE]"));

    try {
        if (xamlIsland) xamlIsland.Close();
    } catch (winrt::hresult_error const&) {
    }
    try {
        if (source) source.Close();
    } catch (winrt::hresult_error const&) {
    }
    if (hwnd) ::DestroyWindow(hwnd);
    try {
        if (manager) manager.Close();
        controller.ShutdownQueue();
    } catch (winrt::hresult_error const&) {
    }
    return expected ? 0 : 1;
}
