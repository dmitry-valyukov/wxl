// Воспроизведение ошибки LinedFlowLayout в WinUI (controls/dev/Repeater/LinedFlowLayout.cpp):
// при нулевом окне просмотра замороженный диапазон пуст (-1..-1), и если у размерного элемента
// изменилась желаемая ширина, ComputeFrozenItemsAndLayout вызывает MeasureItemRangeRegularPath(-1, -1);
// ElementManager::GetRealizedElement(-1) даёт E_BOUNDS (0xC000027B) или чтение за вектором (0xC0000005).
//
// Воспроизводится на голом cppwinrt, без wxl.ui: ItemsView 500x400, LinedFlowLayout, ItemContainer с
// картинкой, 13 элементов. Запуск: sandbox.items-view-probe.exe page strip nav swap zero -- высота
// ItemsView 0 на первые 1,5 секунды, падает каждый раз; без `zero` -- когда загрузка картинок обгоняет
// первую раскладку. Образец Gallery обходит её так же, как оригинал: ItemsSource ставится после
// Loaded через DispatcherQueue с низким приоритетом.
//
// Остальные ключи -- режимы, которыми проба сужалась. Первый ключ: page -- страница с тремя ItemsView
// и RadioButtons как у Gallery, keep/factory/xaml/stack/bare -- один ItemsView; дальше strip (плашка в
// шаблоне), nav (NavigationView, окно 1280x800), swap (Mica, подмена страницы), zero, opts (NumberBox
// и RadioButtons меняют lined), relay (смена layout на StackLayout и обратно). Код выхода 0 -- не упало.

#include "platform.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Media.Imaging.h>

#include <impl/bootstrap.h>
#include <impl/hresult.h>

#include <chrono>
#include <limits>
#include <cstdio>
#include <string>
#include <vector>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
namespace wf = winrt::Windows::Foundation;

namespace {

bool g_xaml = false;
bool g_keep = false;
bool g_stack = false;
bool g_bare = false;
bool g_page = false;
bool g_strip = false;
bool g_nav = false;
bool g_swap = false;
bool g_zero = false;
bool g_opts = false;
bool g_relay = false;

std::wstring imagePath(int64_t index) {
    return L"M:/worktrees/wxl-gallery/samples/Gallery/Assets/SampleMedia/LandscapeImage" + std::to_wstring(index % 13 + 1) + L".jpg";
}

Controls::ItemContainer build(int64_t index) {
    Controls::Image image;
    image.Stretch(Media::Stretch::UniformToFill);
    image.MinWidth(70);
    Media::Imaging::BitmapImage bitmap;
    bitmap.UriSource(wf::Uri {L"file:///" + imagePath(index)});
    image.Source(bitmap);
    Controls::Grid grid;
    grid.Children().Append(image);
    if (g_strip) {
        Controls::TextBlock title;
        title.Text(L"Item " + std::to_wstring(index));
        Controls::TextBlock likes;
        likes.Text(std::to_wstring(index * 7) + L" Likes");
        Controls::StackPanel line;
        line.Orientation(Controls::Orientation::Horizontal);
        line.Children().Append(likes);
        Controls::StackPanel strip;
        strip.Height(40);
        strip.Padding(Thickness {5, 1, 5, 1});
        strip.VerticalAlignment(VerticalAlignment::Bottom);
        strip.Background(Media::SolidColorBrush {winrt::Windows::UI::Color {255, 40, 40, 40}});
        strip.Opacity(0.75);
        strip.Orientation(Controls::Orientation::Vertical);
        strip.Children().Append(title);
        strip.Children().Append(line);
        grid.Children().Append(strip);
    }
    Controls::ItemContainer container;
    container.Child(grid);
    return container;
}

struct Factory : implements<Factory, IElementFactory> {
    UIElement GetElement(ElementFactoryGetArgs const& args) {
        return build(unbox_value<int64_t>(args.Data()));
    }
    std::vector<UIElement> kept;
    void RecycleElement(ElementFactoryRecycleArgs const& args) {
        if (g_keep) {
            kept.push_back(args.Element());
        }
    }
};

struct App : ApplicationT<App, Markup::IXamlMetadataProvider> {
    auto GetXamlType(winrt::Windows::UI::Xaml::Interop::TypeName const& type) const { return provider_.GetXamlType(type); }
    auto GetXamlType(hstring const& name) const { return provider_.GetXamlType(name); }
    auto GetXmlnsDefinitions() const { return provider_.GetXmlnsDefinitions(); }
    XamlTypeInfo::XamlControlsXamlMetaDataProvider provider_;

    Window window {nullptr};
    Microsoft::UI::Dispatching::DispatcherQueueTimer timeout {nullptr};

    // The Gallery page: three ItemsViews one under another; the second is the swappable one, whose
    // RadioButtons (selectedIndex = 0) change the layout and the item template as the page of the
    // original does, the first selection event included.
    void buildPage() {
        auto const itemsOf = [] {
            auto items = single_threaded_vector<wf::IInspectable>();
            for (int64_t i = 0; i < 13; ++i) {
                items.Append(box_value(i));
            }
            return items;
        };

        Controls::ItemsView basic;
        basic.Width(220);
        basic.Height(400);
        basic.HorizontalAlignment(HorizontalAlignment::Left);
        basic.ItemTemplate(make<Factory>());
        basic.ItemsSource(itemsOf());

        Controls::LinedFlowLayout lined;
        lined.ItemsStretch(Controls::LinedFlowLayoutItemsStretch::Fill);
        lined.LineHeight(160);
        lined.LineSpacing(5);
        lined.MinItemSpacing(5);
        Controls::UniformGridLayout grid;
        grid.MinRowSpacing(5);
        grid.MinColumnSpacing(5);
        grid.MaximumRowsOrColumns(3);
        Controls::StackLayout stack;
        stack.Spacing(5);

        Controls::ItemsView swappable;
        swappable.Width(500);
        swappable.Height(400);
        swappable.HorizontalAlignment(HorizontalAlignment::Left);
        swappable.Layout(lined);
        swappable.ItemTemplate(make<Factory>());
        swappable.ItemsSource(itemsOf());

        Controls::RadioButtons choices;
        choices.Header(box_value(L"Layout"));
        for (auto const name : {L"LinedFlowLayout", L"UniformGridLayout", L"StackLayout"}) {
            Controls::RadioButton button;
            button.Content(box_value(name));
            choices.Items().Append(button);
        }
        choices.SelectionChanged([swappable, lined, grid, stack, choices](auto&&, auto&&) {
            auto const choice = choices.SelectedIndex();
            if (choice == 0) { swappable.Layout(lined); }
            if (choice == 1) { swappable.Layout(grid); }
            if (choice == 2) { swappable.Layout(stack); }
            swappable.ItemTemplate(make<Factory>());
        });
        choices.SelectedIndex(0);

        Controls::ItemsView invocation;
        invocation.Width(500);
        invocation.Height(400);
        invocation.HorizontalAlignment(HorizontalAlignment::Left);
        Controls::UniformGridLayout grid2;
        grid2.MinRowSpacing(5);
        grid2.MinColumnSpacing(5);
        grid2.MaximumRowsOrColumns(3);
        invocation.Layout(grid2);
        invocation.ItemTemplate(make<Factory>());
        invocation.ItemsSource(itemsOf());

        Controls::StackPanel row;
        row.Orientation(Controls::Orientation::Horizontal);
        row.Children().Append(swappable);
        Controls::StackPanel side;
        side.Children().Append(choices);
        if (g_opts) {
            auto const number = [](hstring const& header, double value, auto apply) {
                Controls::NumberBox box;
                box.Header(box_value(header));
                box.Minimum(0);
                box.Maximum(100);
                box.Value(value);
                box.SpinButtonPlacementMode(Controls::NumberBoxSpinButtonPlacementMode::Inline);
                box.SmallChange(1);
                box.MaxWidth(250);
                box.ValueChanged([apply](auto&& sender, auto&&) { apply(sender.Value()); });
                return box;
            };
            side.Children().Append(number(L"Space between lines", 5, [lined](double v) { lined.LineSpacing(v); }));
            side.Children().Append(number(L"Minimum space between items on a line", 5, [lined](double v) { lined.MinItemSpacing(v); }));
            Controls::RadioButtons height;
            height.Header(box_value(L"Line height"));
            for (auto const name : {L"Small", L"Large"}) {
                Controls::RadioButton button;
                button.Content(box_value(name));
                height.Items().Append(button);
            }
            height.SelectionChanged([lined, height](auto&&, auto&&) {
                if (height.SelectedIndex() >= 0) { lined.LineHeight(height.SelectedIndex() == 0 ? 80.0 : 160.0); }
            });
            height.SelectedIndex(1);
            side.Children().Append(height);
        }
        row.Children().Append(side);

        if (g_zero) {
            swappable.Height(0);
        }
        Controls::StackPanel page;
        page.Children().Append(basic);
        page.Children().Append(row);
        page.Children().Append(invocation);
        Controls::ScrollViewer scroll;
        scroll.Content(page);

        window = Window {};
        if (g_nav) {
            Controls::NavigationView nav;
            nav.PaneDisplayMode(Controls::NavigationViewPaneDisplayMode::Left);
            if (g_swap) {
                Controls::TextBlock home;
                home.Text(L"Home");
                nav.Content(home);
                window.SystemBackdrop(Media::MicaBackdrop {});
                window.ExtendsContentIntoTitleBar(true);
            }
            nav.Content(scroll);
            window.Content(nav);
            window.AppWindow().Resize({1280, 800});
        } else {
            window.Content(scroll);
        }
        window.Activate();

        timeout = window.DispatcherQueue().CreateTimer();
        timeout.Interval(std::chrono::seconds {12});
        timeout.Tick([this](auto&&, auto&&) {
            window.Close();
            Exit();
        });
        timeout.Start();

        if (g_relay) {
            restore = window.DispatcherQueue().CreateTimer();
            restore.Interval(std::chrono::milliseconds {1500});
            restore.IsRepeating(true);
            restore.Tick([this, swappable, lined, stack](auto&&, auto&&) {
                if (++relayStep == 1) {
                    swappable.Layout(stack);
                } else if (relayStep == 2) {
                    swappable.Layout(lined);
                    restore.Stop();
                }
            });
            restore.Start();
        }
        if (g_zero) {
            restore = window.DispatcherQueue().CreateTimer();
            restore.Interval(std::chrono::milliseconds {1500});
            restore.IsRepeating(false);
            restore.Tick([swappable](auto&&, auto&&) { swappable.Height(400); });
            restore.Start();
        }
    }

    Microsoft::UI::Dispatching::DispatcherQueueTimer restore {nullptr};
    int relayStep = 0;

    void OnLaunched(LaunchActivatedEventArgs const&) {
        Resources().MergedDictionaries().Append(Controls::XamlControlsResources {});
        UnhandledException([](auto&&, UnhandledExceptionEventArgs const& e) {
            std::printf("UnhandledException: 0x%08X  %ls\n", static_cast<unsigned>(e.Exception().value), e.Message().c_str());
            std::fflush(stdout);
            e.Handled(true);
        });
        if (g_page) {
            buildPage();
            return;
        }
        Controls::ItemsView view;
        view.Width(500);
        view.Height(400);
        view.HorizontalAlignment(HorizontalAlignment::Left);

        Controls::LinedFlowLayout lined;
        lined.ItemsStretch(Controls::LinedFlowLayoutItemsStretch::Fill);
        lined.LineHeight(160);
        lined.LineSpacing(5);
        lined.MinItemSpacing(5);
        if (g_stack) {
            Controls::StackLayout stack;
            stack.Spacing(5);
            view.Layout(stack);
        } else {
            view.Layout(lined);
        }

        if (g_bare) {
        } else if (g_xaml) {
            view.ItemTemplate(Markup::XamlReader::Load(
                LR"(<DataTemplate xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation">
                    <ItemContainer><Grid><Image Stretch="UniformToFill" MinWidth="70" Source="{Binding}"/></Grid></ItemContainer>
                </DataTemplate>)").as<DataTemplate>());
        } else {
            view.ItemTemplate(make<Factory>());
        }

        auto items = single_threaded_vector<wf::IInspectable>();
        for (int64_t i = 0; i < 13; ++i) {
            items.Append(box_value(i));
        }
        view.ItemsSource(items);

        window = Window {};
        window.Content(view);
        window.Activate();

        timeout = window.DispatcherQueue().CreateTimer();
        timeout.Interval(std::chrono::seconds {4});
        timeout.Tick([this](auto&&, auto&&) {
            window.Close();
            Exit();
        });
        timeout.Start();
    }
};

}  // namespace

int main(int argc, char** argv) {
    g_xaml = argc > 1 && std::string {argv[1]} == "xaml";
    g_keep = argc > 1 && std::string {argv[1]} == "keep";
    g_stack = argc > 1 && std::string {argv[1]} == "stack";
    g_bare = argc > 1 && std::string {argv[1]} == "bare";
    g_page = argc > 1 && std::string {argv[1]} == "page";
    g_strip = argc > 2 && std::string {argv[2]} == "strip";
    g_nav = argc > 3 && std::string {argv[3]} == "nav";
    g_swap = argc > 4 && std::string {argv[4]} == "swap";
    g_zero = argc > 5 && std::string {argv[5]} == "zero";
    g_opts = argc > 6 && std::string {argv[6]} == "opts";
    g_relay = argc > 7 && std::string {argv[7]} == "relay";
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
