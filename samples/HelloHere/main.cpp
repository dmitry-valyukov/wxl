// A complete WXL application: a single function containing the entire UI definition.
// No wWinMain, no Windows App Runtime bootstrap, no Application subclass, and
// no XAML -- WXL manages the lifecycle infrastructure and invokes this entry point.

#include "pch.h"

using namespace wxl;
using namespace wxl::dsl;

// Presets are their arguments, and these are constants: laid out by the
// compiler, with nothing to run for them at start-up.
namespace presets {
    inline constexpr Preset centeredAndWrapped{hAlign.center, textWrapping.wrap};
    inline constexpr Preset allCentered{hAlign.center, vAlign.center};
}

ElementTheme nextTheme(ElementTheme value) {
    constexpr int themeCount = 3;
    return ElementTheme {(int(value) + 1) % themeCount};
}

FluentSymbol themeIcon(ElementTheme value)
{
    switch(value) {
    case ElementTheme::Light:
        return FluentSymbol::Brightness;
    case ElementTheme::Dark:
        return FluentSymbol::MobQuietHours;
    default:
        return FluentSymbol::Settings;
    }
}

std::wstring themeName(ElementTheme value)
{
    switch(value) {
    case ElementTheme::Light:
        return L"Light";
    case ElementTheme::Dark:
        return L"Dark";
    default:
        return L"Default System";
    }
}

ElementTheme currentTheme = ElementTheme::Default;

void updateThemeButton(Button const& themeButton) {
    auto const next = nextTheme(currentTheme);
    Apply {themeButton,
        content = SymbolIcon {symbol = themeIcon(next)},
        toolTip = L"Switch theme to " + themeName(next),
    };
}

void updateTheme(Button const& themeButton) {
    if (auto const root = themeButton.xamlRoot().content().try_as<FrameworkElement>())
        root.requestedTheme(currentTheme);

    updateThemeButton(themeButton);
}

wxl::Teardown wxl_launched() {
    auto themeButton = Button {
        Margin {16, 0},
        onClick = [&](Button const& self) {
            currentTheme = nextTheme(currentTheme);
            updateTheme(self);
        },
    };

    auto topBar = Card {
        row = 0,
        hAlign.stretch,
        CornerRadius {2},
        Padding {16, 8},
        Grid {
            HtmlBlock {
                fontFamily = L"Segoe UI",
                fontSize = 28,
                FontWeight {600},
                vAlign.center,
                isTextSelectionEnabled = false,
                // The markup string must be placed last: setters are evaluated from left to right,
                // and the <sub> tag scales relative to the previously applied font size.
                L"<font color=\"#0080ff\">W</font><sub>inUI</sub> "
                L"<font color=\"#ff9d00\">X</font><sub>aml</sub>-"
                L"<font color=\"#00a053\">L</font><sub>ess</sub>",
            },
            StackPanel {
                orientation.horizontal,
                hAlign.right,
                themeButton,
                HyperlinkButton {
                    content = L" Github Project Repository ",
                    navigateUri = L"https://github.com/dmitry-valyukov/wxl",
                    styles.ButtonBase.TextBlockButton,
                    Padding {3},
                },
            },
        },
    };

    auto card = Card {
        row = 1,
        width = 450,
        height = 310,
        vAlign.center,
        CornerRadius {12},
        Padding {0},
        background = brushes.SolidBackgroundFillColor.Secondary,
        Grid {
            rowDefinitions = L"auto,*,auto",
            StackPanel {
                row = 0,
                orientation.horizontal,
                Padding {110, 0},
                Image {
                    source = L"Assets/wxl.png",
                    Margin {8, 0},
                    width = 78,
                    height = 73.3333333,
                },
                TextBlock {
                    text = L"WXL",
                    styles.TextBlock.Header,
                    vAlign.center,
                    hAlign.center,
                    FontWeight {600},
                },
            },
            Rows {
                row = 1,
                background = brushes.Card.BackgroundFillColor.Default,
                TextBlock {
                    L"A modern C++23 library for declarative, Flutter-style WinUI 3 development.",
                    Margin { 16, 0 },
                    vAlign.bottom,
                    styles.TextBlock.Subtitle,
                    presets::centeredAndWrapped,
                },
                TextBlock {
                    L"Leveraging C++ modules to eliminate heavy WinRT bloat, "
                    L"it restores simplicity and control to native C++ GUI development.",
                    Margin { 16, 14 },
                    vAlign.center,
                    presets::centeredAndWrapped,
                },
            },
            Button {
                row = 2,
                Margin {0, 12},
                content = L"Click",
                onClick = {content = L"Thank You!"},
                hAlign.center,
            },
        },
    };

    auto window = Window {
        title = L"Hello from WXL",
        minSize = {500, 420},
        Grid {
            rowDefinitions = L"auto,*,auto",
            background = brushes.SolidBackgroundFillColor.Base,
            topBar,
            card,
        },
    };

    auto appWindow = window.appWindow();
    appWindow.resize({800, 600});
    appWindow.setIcon(L"Assets/WinUl-logo.ico");
    window.activate();

    currentTheme = themeButton.actualTheme();
    updateThemeButton(themeButton);

    return {};
}
