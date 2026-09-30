// Страница настроек — SettingsPage оригинала: тема, стиль навигации, очистка
// недавних и избранных, сведения о приложении.
//
// SettingsCard и SettingsExpander — типы Community Toolkit, которых у wxl
// нет; строка настройки здесь — Border с подписью слева и элементом справа.
// Настройка звука оригинала (ElementSoundPlayer) не перенесена.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Строка настройки: значок, название с пояснением, элемент управления.
FrameworkElement settingRow(std::u16string_view icon, std::u16string_view label,
                            std::u16string_view details, FrameworkElement const& control) {
    auto texts = StackPanel {
        vAlign.center,
        TextBlock {label},
    };
    if (!details.empty()) {
        texts.children().append(TextBlock {
            details,
            styles.TextBlock.Caption,
            foreground = brushes.Text.FillColor.Secondary,
        });
    }
    return Border {
        Padding {16},
        background = brushes.Control.FillColor.Default,
        borderBrush = brushes.Card.StrokeColorDefault,
        BorderThickness {1},
        CornerRadius {4},
        Grid {
            columnDefinitions = u"auto,*,auto",
            columnSpacing = 16.0,
            FontIcon {glyph = icon, vAlign.center},
            Border {column = 1, texts},
            Border {column = 2, vAlign.center, control},
        },
    };
}

FrameworkElement sectionHeader(std::u16string_view label) {
    return TextBlock {label, Margin {1, 30, 0, 6}, styles.TextBlock.BodyStrong};
}

FrameworkElement themeChoice() {
    return ComboBox {
        ComboBoxItem {content = u"Light"},
        ComboBoxItem {content = u"Dark"},
        ComboBoxItem {content = u"Use system setting"},
        selectedIndex = 2,
        onSelectionChanged = [](Object const& sender, SelectionChangedEventArgs&) {
            auto const box = sender.try_as<ComboBox>();
            auto const root = box.xamlRoot().content().try_as<FrameworkElement>();
            if (!root) {
                return;
            }
            switch (box.selectedIndex()) {
            case 0:
                root.requestedTheme(ElementTheme::Light);
                break;
            case 1:
                root.requestedTheme(ElementTheme::Dark);
                break;
            default:
                root.requestedTheme(ElementTheme::Default);
            }
        },
    };
}

FrameworkElement manageSamples() {
    return StackPanel {
        orientation.horizontal,
        spacing = 8.0,
        Button {
            u"Clear recents",
            minWidth = 120,
            isEnabled = !gallery::recentlyVisited().empty(),
            onClick = [](Object const& sender, RoutedEventArgs&) {
                gallery::clearRecentlyVisited();
                sender.try_as<Button>().isEnabled(false);
            },
        },
        Button {
            u"Remove favorites",
            minWidth = 120,
            isEnabled = !gallery::favorites().empty(),
            onClick = [](Object const& sender, RoutedEventArgs&) {
                gallery::clearFavorites();
                sender.try_as<Button>().isEnabled(false);
            },
        },
    };
}

}  // namespace

wxl::FrameworkElement gallery::settingsPage() {
    return Grid {
        rowDefinitions = u"auto,*",
        TextBlock {u"Settings", Margin {36, 24, 36, 0}, maxWidth = 1064, styles.TextBlock.Title},
        ScrollViewer {
            row = 1,
            Padding {36, 0},
            content = StackPanel {
                maxWidth = 1064,
                spacing = 4.0,
                sectionHeader(u"Appearance & behavior"),
                settingRow(u"", u"App theme", u"Select which app theme to display", themeChoice()),
                settingRow(u"", u"Manage samples", u"Clear your recent or favorite samples", manageSamples()),
                sectionHeader(u"About"),
                settingRow(u"", u"WinUI 3 Gallery",
                           u"A port of the WinUI 3 Gallery to wxl: the same types, no XAML.",
                           HyperlinkButton {
                               u"WinUI Gallery on GitHub",
                               navigateUri = u"https://github.com/microsoft/WinUI-Gallery",
                           }),
            },
        },
    };
}
