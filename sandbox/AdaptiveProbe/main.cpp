// Проба D по AdaptiveTrigger (2026-10-05). Вопросы и ответы:
//   1. срабатывает ли AdaptiveTrigger у VisualStateGroup, собранной из кода, без единой строки XAML? ДА, если корень групп --
//      содержимое UserControl или Page (`uc`, `page`, `deep`: окно -> Grid -> Border -> UserControl -> корень). Если корень лежит прямо в
//      Window или в Border (`code`, `attached`), триггер не срабатывает НИ из кода, НИ из XAML-текста (`xaml`).
//   2. какие Setter работают из кода? Только с TargetPropertyPath, у которого заданы Path (PropertyPath, например L"Margin",
//      L"Text", L"(Grid.Column)", L"Style") и Target (элемент): так делает разборщик XAML. Setter(DependencyProperty, value) и
//      TargetPropertyPath(DependencyProperty) состояние входит, а значения не применяются и ошибки нет (`oldsetters`).
//      Работают: свойство корня, свойство другого элемента, присоединённое свойство (`extras`: Grid.Column = 1), значение-Style.
//   3. откат: когда окно снова узкое, значения возвращаются сами. Состояние без триггера (Narrow) заводить не нужно.
//   4. имя VisualState из кода не задать (Name только для чтения), а триггерам оно не нужно; GoToState по имени невозможен.
//
// PROBE_OPTS (слова через запятую): xaml, hybrid, uc, page, deep, attached, extras, oldsetters, nowidth (контроль: невыполнимое
// условие), late, nosetters. Окно ставится в 1000, 500, 900, 500; после каждого шага через 1,2 с печатается, что видно на корне и
// на TextBlock T. Ответы идут в stdout.
#include "platform.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

#include <impl/bootstrap.h>
#include <impl/hresult.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace {

std::set<std::string> opts;
bool has(char const* name) { return opts.contains(name); }

// Корень страницы: Grid с TextBlock T; поле Margin корня и текст T — то, что меняют состояния.
constexpr wchar_t const* narrowText = L"narrow";
constexpr wchar_t const* wideText = L"wide";

Border U {nullptr};  // второй ребёнок корня для проверки присоединённого свойства

FrameworkElement buildFromCode(TextBlock& t) {
    Grid root;
    root.Margin(ThicknessHelper::FromUniformLength(5));
    t = TextBlock {};
    t.Text(narrowText);
    root.Children().Append(t);
    if (has("extras")) {
        ColumnDefinition a, b;
        root.ColumnDefinitions().Append(a);
        root.ColumnDefinitions().Append(b);
        U = Border {};
        Grid::SetColumn(U, 0);
        root.Children().Append(U);
    }

    // Узкое состояние — без триггеров: оно действует, пока широкое не сработало. Имя VisualState из кода не задать (Name только
    // для чтения: его ставит x:Name), так что GoToState по имени для такого состояния невозможен; триггерам имя не нужно.
    VisualState narrow;

    VisualState wide;
    AdaptiveTrigger trigger;
    if (has("nowidth")) {
        trigger.MinWindowHeight(100000);  // условие, которое выполнить нельзя: контроль, что триггер вообще что-то решает
    } else {
        trigger.MinWindowWidth(641);
    }
    wide.StateTriggers().Append(trigger);

    if (has("oldsetters")) {
        // Так не работает (проба 2026-10-05): Setter(DependencyProperty, value) и TargetPropertyPath(DependencyProperty) состояние
        // входит, а значения не применяются.
        wide.Setters().Append(Setter {FrameworkElement::MarginProperty(), box_value(ThicknessHelper::FromUniformLength(40))});
        Setter onChild;
        TargetPropertyPath path {TextBlock::TextProperty()};
        path.Target(t);
        onChild.Target(path);
        onChild.Value(box_value(wideText));
        wide.Setters().Append(onChild);
    } else {
        // Так работает: TargetPropertyPath с путём (Path) и элементом-целью (Target), как делает разборщик XAML.
        auto const make = [](Windows::Foundation::IInspectable const& target, hstring const& path, Windows::Foundation::IInspectable const& value) {
            Setter setter;
            TargetPropertyPath where;
            where.Path(PropertyPath {path});
            where.Target(target);
            setter.Target(where);
            setter.Value(value);
            return setter;
        };
        wide.Setters().Append(make(root, L"Margin", box_value(ThicknessHelper::FromUniformLength(40))));
        wide.Setters().Append(make(t, L"Text", box_value(wideText)));
        if (has("extras")) {
            // Присоединённое свойство (путь в скобках) и значение-Style: то, что ставят состояния оригинала (Grid.Row/Column, ItemContainerStyle).
            wide.Setters().Append(make(U, L"(Grid.Column)", box_value(1)));
            Style big {winrt::Windows::UI::Xaml::Interop::TypeName {L"Microsoft.UI.Xaml.Controls.TextBlock", winrt::Windows::UI::Xaml::Interop::TypeKind::Metadata}};
            big.Setters().Append(Setter {TextBlock::FontSizeProperty(), box_value(30.0)});
            wide.Setters().Append(make(t, L"Style", big));
        }
    }

    VisualStateGroup group;
    group.States().Append(narrow);
    group.States().Append(wide);
    group.CurrentStateChanged([](auto&&, VisualStateChangedEventArgs const& e) {
        std::printf("  [group] state changed: %s -> %s\n", e.OldState() ? "(a state)" : "(none)", e.NewState() ? "(a state)" : "(none)");
        std::fflush(stdout);
    });
    VisualStateManager::GetVisualStateGroups(root).Append(group);
    return root;
}

// Гибрид: состояния и триггер — из короткого текста XAML (там у состояний есть имена), Setter-ы — из кода.
FrameworkElement buildHybrid(TextBlock& t) {
    Grid root;
    root.Margin(ThicknessHelper::FromUniformLength(5));
    t = TextBlock {};
    t.Text(narrowText);
    root.Children().Append(t);

    auto const group = Markup::XamlReader::Load(
                           L"<VisualStateGroup xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' "
                           L"xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml'>"
                           L"<VisualState x:Name='Narrow'/>"
                           L"<VisualState x:Name='Wide'><VisualState.StateTriggers><AdaptiveTrigger MinWindowWidth='641'/></VisualState.StateTriggers>"
                           L"</VisualState></VisualStateGroup>")
                           .as<VisualStateGroup>();
    auto const wide = group.States().GetAt(1);
    if (has("pathset")) {
        // Setter состояния — как его делает разборщик XAML: TargetPropertyPath с путём к свойству и элементом-целью.
        auto const make = [](Windows::Foundation::IInspectable const& target, hstring const& path, Windows::Foundation::IInspectable const& value) {
            Setter setter;
            TargetPropertyPath where;
            where.Path(PropertyPath {path});
            where.Target(target);
            setter.Target(where);
            setter.Value(value);
            return setter;
        };
        wide.Setters().Append(make(root, L"Margin", box_value(ThicknessHelper::FromUniformLength(40))));
        wide.Setters().Append(make(t, L"Text", box_value(wideText)));
    } else if (!has("nosetters")) {
        wide.Setters().Append(Setter {FrameworkElement::MarginProperty(), box_value(ThicknessHelper::FromUniformLength(40))});
        Setter onChild;
        TargetPropertyPath path {TextBlock::TextProperty()};
        path.Target(t);
        onChild.Target(path);
        onChild.Value(box_value(wideText));
        wide.Setters().Append(onChild);
    }
    // Состояние меняется независимо от того, применились ли Setter-ы.
    group.CurrentStateChanged([](auto&&, VisualStateChangedEventArgs const& e) {
        std::printf("  [group] state changed: %ls -> %ls\n", e.OldState() ? e.OldState().Name().c_str() : L"(none)",
                    e.NewState() ? e.NewState().Name().c_str() : L"(none)");
        std::fflush(stdout);
    });
    if (has("late")) {
        // Группа дописывается после загрузки корня, а не до показа.
        root.Loaded([root, group](auto&&, auto&&) { VisualStateManager::GetVisualStateGroups(root).Append(group); });
    } else {
        VisualStateManager::GetVisualStateGroups(root).Append(group);
    }
    return root;
}

FrameworkElement buildFromXaml(TextBlock& t) {
    auto const root = Markup::XamlReader::Load(
                          L"<Grid x:Name='G' Margin='5' xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' "
                          L"xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml'>"
                          L"<VisualStateManager.VisualStateGroups><VisualStateGroup>"
                          L"<VisualState x:Name='Narrow'/>"
                          L"<VisualState x:Name='Wide'><VisualState.StateTriggers><AdaptiveTrigger MinWindowWidth='641'/></VisualState.StateTriggers>"
                          L"<VisualState.Setters><Setter Target='G.Margin' Value='40'/><Setter Target='T.Text' Value='wide'/></VisualState.Setters>"
                          L"</VisualState></VisualStateGroup></VisualStateManager.VisualStateGroups>"
                          L"<TextBlock x:Name='T' Text='narrow'/></Grid>")
                          .as<Grid>();
    t = root.FindName(L"T").as<TextBlock>();
    return root;
}

struct App : ApplicationT<App> {
    Window window {nullptr};
    FrameworkElement root {nullptr};
    TextBlock t {nullptr};
    Microsoft::UI::Dispatching::DispatcherQueueTimer timer {nullptr};
    int step = 0;

    void report(char const* when) {
        std::printf("%-28s root width %5.0f  root margin %4.0f  T.Text = %ls", when, root.ActualWidth(), root.Margin().Left, t.Text().c_str());
        if (U) {
            std::printf("  U column %d  T.FontSize %.0f", Grid::GetColumn(U), t.FontSize());
        }
        std::printf("\n");
        std::fflush(stdout);
    }

    void resize(int width) { window.AppWindow().Resize({width, 600}); }

    void OnLaunched(LaunchActivatedEventArgs const&) {
        window = Window {};
        root = has("xaml") ? buildFromXaml(t) : has("hybrid") ? buildHybrid(t) : buildFromCode(t);
        if (has("deep")) {
            // Как в Gallery: окно → Grid → NavigationView-подобный хост (Border) → UserControl → корень групп.
            UserControl control;
            control.Content(root);
            Border host;
            host.Child(control);
            Grid outer;
            outer.Children().Append(host);
            window.Content(outer);
        } else if (has("uc")) {
            // Корень групп — содержимое UserControl: так устроены Page и UserControl оригинала (неявный корень состояний).
            UserControl control;
            control.Content(root);
            window.Content(control);
        } else if (has("page")) {
            Page page;
            page.Content(root);
            window.Content(page);
        } else if (has("attached")) {
            Border host;
            host.Child(root);
            Grid outer;
            outer.Children().Append(host);
            window.Content(outer);
        } else {
            window.Content(root);
        }
        resize(1000);
        window.Activate();

        timer = window.DispatcherQueue().CreateTimer();
        timer.Interval(std::chrono::milliseconds {1200});
        timer.IsRepeating(true);
        timer.Tick([this](auto&&, auto&&) {
            switch (step++) {
            case 0: report("start, window 1000"); resize(500); break;
            case 1: report("window 500 (expect narrow)"); resize(900); break;
            case 2: report("window 900 (expect wide)"); resize(500); break;
            case 3: report("window 500 again (expect narrow)"); break;
            default:
                timer.Stop();
                window.Close();
                Exit();
            }
        });
        timer.Start();
    }
};

}  // namespace

int main() {
    if (char const* const o = std::getenv("PROBE_OPTS")) {
        std::string word;
        for (char const* p = o;; ++p) {
            if (*p == ',' || *p == 0) {
                if (!word.empty()) opts.insert(word);
                word.clear();
                if (*p == 0) break;
            } else {
                word += *p;
            }
        }
    }
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
