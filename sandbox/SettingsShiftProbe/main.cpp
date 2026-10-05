// Проба смещения страницы настроек Gallery. SHIFT_OPTS (слова через запятую):
//   top        PaneDisplayMode Top (иначе Auto)      view      ScrollView вместо ScrollViewer
//   border     Border вокруг StackPanel              plain     простые карточки вместо SettingsCard
//   xaml       страница из текста XAML (карточки простые)
//   center     StackPanel hAlign.center              nomax     без maxWidth
//   late       страница ставится после показа окна   resizelate  размер окна 1280x800 после Activate
//   noexp      без SettingsExpander                   short     без карточки с длинным текстом
//   lit        SettingsCard со строками-литералами    u16 / emptydesc   проверки SettingsCard (см. ниже)
// Результат пишется в файл SHIFT_LOG.
//
// Что выяснено (2026-10-05). Стек с MaxWidth=1064 и растяжением внутри ScrollViewer в режиме Top (окно шире 1064 + поля)
// рисуется со сдвигом, если требуемая ширина стека меньше MaxWidth: смещение равно (viewport - требуемая ширина) / 2, а
// рисуется он шириной 1064. Тот же сдвиг даёт страница из чистого XAML (вариант `top,xaml,short`: x=495.5 против 100),
// значит это поведение WinUI, не wxl. Убирают его ScrollView вместо ScrollViewer или Border вокруг стека (`view`, `border`),
// как у оригинала. В режиме Auto окно уже MaxWidth, ограничение не действует и сдвига нет.
// Отдельный дефект (исправлен): SettingsCard с пустым заголовком или описанием падал при первой раскладке (варианты `emptyhdr`,
// `emptydesc`); теперь пустая строка, как и null, сворачивает часть, как в Toolkit.
// Диагностика: launch.cpp печатает сообщение необработанной ошибки XAML в stderr (в этом случае: 0x80040111,
// Windows.ApplicationModel.LimitedAccessFeatures).

#include "ui.h"
#include "SettingsCard.h"
#include "SettingsExpander.h"
#include "launch.h"
#include <wxl/Microsoft.UI.Xaml.Media.h>
#include <wxl/Microsoft.UI.Dispatching.h>
#include <wxl/brushes.h>
#include "LoadXaml.h"
#include "StringList.h"

import std;

using namespace wxl;
using namespace wxl::dsl;

namespace {

std::set<std::string> opts;
bool has(char const* name) { return opts.contains(name); }

std::FILE* logFile;
void L(char const* s) { std::fprintf(logFile, "step: %s\n", s); std::fflush(logFile); }

void walk(char const* label, FrameworkElement const& e, FrameworkElement const& root, int depth) {
    if (!e) return;
    auto const p = e.transformToVisual(root).transformPoint({0, 0});
    std::fprintf(logFile, "%*s%s actual=%.0fx%.0f desired=%.0fx%.0f  x=%.1f y=%.1f  align=%d\n", depth * 2, "", label, e.actualWidth(),
                 e.actualHeight(), e.desiredSize().width, e.desiredSize().height, p.x, p.y, int(e.horizontalAlignment()));
    if (e.is<ScrollViewer>()) {
        auto const sv = e.try_as<ScrollViewer>();
        std::fprintf(logFile, "%*s  viewport=%.0f hOffset=%.0f\n", depth * 2, "", sv.viewportWidth(), sv.horizontalOffset());
    }
}

std::wstring plainCard(std::wstring header, std::wstring description) {
    return L"<Border Background='#22888888' CornerRadius='4' Padding='16,12' MinHeight='68'><Grid>"
           L"<Grid.ColumnDefinitions><ColumnDefinition Width='*'/><ColumnDefinition Width='Auto'/></Grid.ColumnDefinitions>"
           L"<StackPanel><TextBlock TextWrapping='WrapWholeWords' Text='" + header + L"'/><TextBlock FontSize='12' TextWrapping='WrapWholeWords' Text='" + description +
           L"'/></StackPanel><ComboBox Grid.Column='1' SelectedIndex='0'><ComboBoxItem Content='Left'/><ComboBoxItem Content='Top'/></ComboBox></Grid></Border>";
}

constexpr wchar_t const* longText = L"THIS CODE AND INFORMATION IS PROVIDED AS IS WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.";

std::wstring xamlPage() {
    std::wstring cards;
    for (int i = 0; i < 4; ++i) cards += plainCard(L"Setting " + std::to_wstring(i), L"Some description of the setting");
    if (!has("short")) cards += plainCard(longText, L"");
    std::wstring stackAttrs = L" x:Name='S' Spacing='4'";
    if (!has("nomax")) stackAttrs += L" MaxWidth='1064'";
    if (has("center")) stackAttrs += L" HorizontalAlignment='Center'";
    std::wstring stack = L"<StackPanel" + stackAttrs + L">" + cards + L"</StackPanel>";
    if (has("border")) stack = L"<Border x:Name='B'>" + stack + L"</Border>";
    std::wstring scroller = has("view") ? L"<ScrollView x:Name='V' Grid.Row='1' Padding='36,0'>" + stack + L"</ScrollView>"
                                        : L"<ScrollViewer x:Name='V' Grid.Row='1' Padding='36,0'>" + stack + L"</ScrollViewer>";
    return L"<Grid x:Name='G' xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml'>"
           L"<Grid.RowDefinitions><RowDefinition Height='Auto'/><RowDefinition Height='*'/></Grid.RowDefinitions>"
           L"<TextBlock Text='Settings' Margin='36,24,36,0' MaxWidth='1064' FontSize='28'/>" + scroller + L"</Grid>";
}

FrameworkElement card(std::u16string head, std::u16string descr) {
    if (has("plain")) {
        return Border {
            background = brushes.Card.BackgroundFillColor.Default,
            CornerRadius {4}, Padding {16, 12}, minHeight = 68,
            Grid {
                columnDefinitions = u"*,auto",
                StackPanel {TextBlock {head, textWrapping.wrapWholeWords}, TextBlock {descr, textWrapping.wrapWholeWords}},
                ComboBox {column = 1, ComboBoxItem {content = u"Left"}, ComboBoxItem {content = u"Top"}, selectedIndex = 0},
            },
        };
    }
    if (has("u16")) {
        // Заголовок — std::u16string, описание — непустой литерал.
        return SettingsCard {header = head, description = u"d", ComboBox {ComboBoxItem {content = u"Left"}, selectedIndex = 0}};
    }
    if (has("spacedesc")) {
        return SettingsCard {header = u"h", description = u" ", ComboBox {ComboBoxItem {content = u"Left"}, selectedIndex = 0}};
    }
    if (has("emptyhdr")) {
        return SettingsCard {header = u"", description = u"d", ComboBox {ComboBoxItem {content = u"Left"}, selectedIndex = 0}};
    }
    if (has("nodesc")) {
        return SettingsCard {header = u"h", ComboBox {ComboBoxItem {content = u"Left"}, selectedIndex = 0}};
    }
    if (has("emptydesc")) {
        // Заголовок — литерал, описание — пустой литерал.
        return SettingsCard {header = u"h", description = u"", ComboBox {ComboBoxItem {content = u"Left"}, selectedIndex = 0}};
    }
    if (has("lit")) {
        // Как в Gallery: строки-литералы, описание не пустое.
        return SettingsCard {
            header = u"Setting", description = u"Some description of the setting",
            ComboBox {ComboBoxItem {content = u"Left"}, ComboBoxItem {content = u"Top"}, selectedIndex = 0},
        };
    }
    return SettingsCard {
        header = head, description = descr,
        ComboBox {ComboBoxItem {content = u"Left"}, ComboBoxItem {content = u"Top"}, selectedIndex = 0},
    };
}

FrameworkElement dslStack() {
    auto s = StackPanel {name = u"S", spacing = 4.0};
    if (!has("nomax")) s.maxWidth(1064);
    if (has("center")) s.horizontalAlignment(HorizontalAlignment::Center);
    if (has("cc") || has("cc2")) {
        // Чистый ContentControl с шаблоном из XAML: пустая строка в Content (cc) и в Content, который ставится после показа (cc2).
        auto const control = loadXaml(
            L"<ContentControl xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'><ContentControl.Template>"
            L"<ControlTemplate TargetType='ContentControl'><ContentPresenter TextWrapping='WrapWholeWords' FontSize='12'/>"
            L"</ControlTemplate></ContentControl.Template></ContentControl>").try_as<ContentControl>();
        if (has("cc")) {
            control.content(stringBox(u""));
        } else {
            control.add_onLoaded([control](auto&&...) { control.content(stringBox(u"")); });
        }
        s.children().append(control);
        return s;
    }
    if (has("cp2") || has("cp3") || has("cp4")) {
        // ContentPresenter из XAML со свойствами из шаблона SettingsCard; пустая строка кладётся в Content из C++.
        std::wstring attrs = has("cp2") ? L" TextWrapping='WrapWholeWords'" : has("cp3") ? L" FontSize='12'" : L" TextWrapping='WrapWholeWords' FontSize='12' Foreground='{ThemeResource TextFillColorSecondaryBrush}'";
        auto const presenter = loadXaml(L"<ContentPresenter xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'" + attrs + L"/>").try_as<ContentPresenter>();
        presenter.content(stringBox(u""));
        s.children().append(presenter);
        return s;
    }
    if (has("cp")) {
        // Голый ContentPresenter с пустой строкой в Content, как у SettingsCard с description = u"".
        s.children().append(ContentPresenter {content = stringBox(u"")});
        return s;
    }
    for (int i = 0; i < 4; ++i) s.children().append(card(u"Setting " + std::u16string(1, char16_t(u'0' + i)), u"Some description of the setting"));
    if (!has("short")) s.children().append(card(u"THIS CODE AND INFORMATION IS PROVIDED AS IS WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.", u""));
    if (!has("noexp") && !has("plain")) {
        s.children().append(SettingsExpander {
            header = u"Sound", description = u"Controls provide audible feedback",
            ToggleSwitch {},
            items[SettingsCard {header = u"Spatial", description = u"Learn more", ToggleSwitch {}}],
        });
    }
    return s;
}

FrameworkElement dslPage();
FrameworkElement buildPage() {
    return has("xaml") ? loadXaml(xamlPage()).try_as<FrameworkElement>() : dslPage();
}

FrameworkElement dslPage() {
    FrameworkElement stack = dslStack();
    if (has("border")) {
        stack = Border {name = u"B", stack};
    }
    FrameworkElement scroller = has("view") ? FrameworkElement(ScrollView {name = u"V", row = 1, Padding {36, 0}, content = stack})
                                            : FrameworkElement(ScrollViewer {name = u"V", row = 1, Padding {36, 0}, content = stack});
    return Grid {
        name = u"G",
        rowDefinitions = u"auto,*",
        TextBlock {u"Settings", Margin {36, 24, 36, 0}, maxWidth = 1064, styles.TextBlock.Title},
        scroller,
    };
}

std::optional<Border> hostBox;
std::optional<FrameworkElement> page;
std::optional<Window> mainWindow;
std::optional<DispatcherQueueTimer> timer;
std::optional<FrameworkElement> navigationView;

void dump(char const* when) {
    L(when);
    auto const root = mainWindow->content().try_as<FrameworkElement>();
    std::fprintf(logFile, "== %s: nav actual=%.0f  host actual=%.0f\n", when, navigationView->actualWidth(), hostBox->actualWidth());
    walk("G", *page, root, 1);
    auto const grid = page->try_as<Panel>();
    auto const v = grid.children().getAt(1).try_as<FrameworkElement>();
    walk("V", v, root, 2);

    // try_as на промахе в Debug — assert (для проверки типа есть is<>).
    std::optional<FrameworkElement> inner;
    if (v.is<ScrollViewer>()) {
        inner = v.try_as<ScrollViewer>().content().try_as<FrameworkElement>();
    } else {
        inner = v.try_as<ScrollView>().content().try_as<FrameworkElement>();
    }
    auto stack = *inner;
    if (stack.is<Border>()) {
        walk("B", stack, root, 3);
        stack = stack.try_as<Border>().child().try_as<FrameworkElement>();
    }
    walk("S", stack, root, 4);
    auto const kids = stack.try_as<Panel>().children();
    for (uint32_t i = 0; i < kids.size(); ++i) {
        auto const kid = kids.getAt(i).try_as<FrameworkElement>();
        walk("child", kid, root, 5);
    }
    std::fflush(logFile);
}

}  // namespace

wxl::Teardown wxl_launched() {
    if (char const* const o = std::getenv("SHIFT_OPTS")) {
        for (auto const word : std::string_view {o} | std::views::split(',')) opts.insert(std::string(std::string_view(word)));
    }
    char const* const path = std::getenv("SHIFT_LOG");
    logFile = std::fopen(path ? path : "shift.log", "w");
    std::fprintf(logFile, "opts: %s\n", std::getenv("SHIFT_OPTS") ? std::getenv("SHIFT_OPTS") : "");

    L("start"); hostBox = Border {};
    auto nav = NavigationView {
        row = 1,
        isBackButtonVisible = NavigationViewBackButtonVisible::Collapsed,
        isPaneToggleButtonVisible = false,
        content = *hostBox,
        paneDisplayMode = has("top") ? NavigationViewPaneDisplayMode::Top : NavigationViewPaneDisplayMode::Auto,
    };
    L("nav built"); navigationView = nav;
    mainWindow = Window {
        title = u"Shift probe",
        content = Grid {rowDefinitions = u"auto,*", TextBlock {u"title bar"}, nav},
    };
    L("window built"); if (!has("resizelate")) mainWindow->appWindow().resize({1280, 800});
    if (!has("late")) {
        page = buildPage();
        hostBox->child(*page);
    }
    L("page set"); mainWindow->activate(); L("activated");
    if (has("resizelate")) mainWindow->appWindow().resize({1280, 800});

    timer = DispatcherQueue::getForCurrentThread().createTimer();
    timer->interval(std::chrono::milliseconds {1500});
    timer->add_onTick([step = 0](auto&&...) mutable {
        if (step == 0 && has("late")) {
            page = buildPage();
            hostBox->child(*page);
        } else if (step < 3) {
            dump(step ? "later" : "after 1.5s");
        } else if (step == 3) {
            // Таймер повторяется: после закрытия окна и файла он больше не должен ничего трогать.
            timer->stop();
            std::fclose(logFile);
            mainWindow->close();
        }
        ++step;
    });
    timer->isRepeating(true);
    timer->start(); L("timer started");
    return {};
}
