// Своя панель на шаблоне cppwinrt PanelT<D>. PanelT в конструкторе зовёт
// IPanelFactory::CreateInstance(*this, &inner): объект — наш, настоящий Panel —
// внутри него, QueryInterface и IFrameworkElementOverrides cppwinrt
// переадресует сам.
//
// Вопросы:
//   1. создаётся ли панель без регистрации своего класса — winrt::make, и никто
//      не спрашивает RoGetActivationFactory о её имени;
//   2. зовёт ли XAML её MeasureOverride и ArrangeOverride;
//   3. встают ли дети туда, куда их поставила ArrangeOverride;
//   4. раскладывается ли панель заново, когда сама объявляет раскладку
//      устаревшей (InvalidateMeasure), — так пойдёт перетаскивание разделителя.
//
// Дети — левая часть, полоса разделителя в зазоре и правая часть. Ответы — в
// stdout, окно закрывается само после второй раскладки.

#include <windows.h>

// Как в sandbox/Aggregation: иначе макрос сталкивается с Storyboard::GetCurrentTime.
#undef GetCurrentTime

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.Numerics.h>
#include <winrt/Windows.UI.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

#include <impl/bootstrap.h>
#include <impl/hresult.h>

#include <algorithm>
#include <chrono>
#include <cstdio>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using winrt::Windows::Foundation::Rect;
using winrt::Windows::Foundation::Size;

namespace {

struct SplitPanel : Controls::PanelT<SplitPanel> {
    // Своё имя: без него cppwinrt отвечает именем первого интерфейса из implements.
    hstring GetRuntimeClassName() const { return L"wxl.SplitPanel"; }

    float left = 200;
    float gap = 8;
    int measures = 0;
    int arranges = 0;

    Size MeasureOverride(Size available) {
        ++measures;
        auto const children = Children();
        float const rest = std::max(0.0f, available.Width - left - gap);
        children.GetAt(0).Measure({left, available.Height});
        children.GetAt(1).Measure({gap, available.Height});
        children.GetAt(2).Measure({rest, available.Height});
        return available;
    }

    Size ArrangeOverride(Size final) {
        ++arranges;
        auto const children = Children();
        float const rest = std::max(0.0f, final.Width - left - gap);
        children.GetAt(0).Arrange(Rect {0, 0, left, final.Height});
        children.GetAt(1).Arrange(Rect {left, 0, gap, final.Height});
        children.GetAt(2).Arrange(Rect {left + gap, 0, rest, final.Height});
        return final;
    }
};

Controls::Border part(winrt::Windows::UI::Color color) {
    Controls::Border border;
    border.Background(Media::SolidColorBrush {color});
    return border;
}

struct App : ApplicationT<App> {
    Window window {nullptr};
    com_ptr<SplitPanel> panel;
    event_token layout;
    int step = 0;

    void report(char const* when) {
        std::printf("%s: MeasureOverride %d, ArrangeOverride %d\n", when, panel->measures, panel->arranges);
        char const* names[] = {"left", "splitter", "right"};
        auto const children = panel->Children();
        for (uint32_t i = 0; i < children.Size(); ++i) {
            auto const element = children.GetAt(i);
            auto const offset = element.ActualOffset();
            auto const size = element.ActualSize();
            std::printf("  %-8s x %6.1f  width %6.1f\n", names[i], offset.x, size.x);
        }
    }

    void OnLaunched(LaunchActivatedEventArgs const&) {
        panel = make_self<SplitPanel>();
        std::printf("make_self<SplitPanel>() without registration: ok\n");
        std::printf("GetRuntimeClassName: %ls\n", get_class_name(panel.as<winrt::Windows::Foundation::IInspectable>()).c_str());

        panel->Children().Append(part({255, 200, 220, 255}));
        panel->Children().Append(part({255, 60, 60, 60}));
        panel->Children().Append(part({255, 220, 255, 200}));

        window = Window {};
        window.Content(*panel);

        // Первая раскладка — со своей шириной левой части; затем левая часть
        // шире на 100, панель сама объявляет раскладку устаревшей, и вторая
        // раскладка должна поставить детей по-новому.
        layout = panel->LayoutUpdated([this](auto&&, auto&&) {
            if (panel->ActualSize().x == 0) {
                return;
            }
            if (step == 0) {
                step = 1;
                report("first layout");
                panel->left += 100;
                panel->InvalidateMeasure();
            } else if (step == 1 && panel->Children().GetAt(1).ActualOffset().x != 200) {
                step = 2;
                report("after InvalidateMeasure, left += 100");
                panel->LayoutUpdated(layout);
                timeout.Stop();
                window.Close();
                Exit();
            }
        });

        // Не дождались второй раскладки — это тоже ответ, и проба не висит.
        timeout = window.DispatcherQueue().CreateTimer();
        timeout.Interval(std::chrono::seconds {5});
        timeout.Tick([this](auto&&, auto&&) {
            report("timeout: no second layout");
            window.Close();
            Exit();
        });
        timeout.Start();

        window.Activate();
    }

    Microsoft::UI::Dispatching::DispatcherQueueTimer timeout {nullptr};
};

}  // namespace

int main() {
    init_apartment(apartment_type::single_threaded);

    try {
        wxl::impl::ensure_windows_app_runtime_initialized();
    } catch (wxl::hresult_error const& e) {
        std::printf("ensure_windows_app_runtime_initialized failed: 0x%08X\n", static_cast<unsigned>(e.code()));
        return 1;
    }

    Application::Start([](auto&&) { make<App>(); });
    return 0;
}
