// Страница настроек — SettingsPage оригинала: тема, стиль навигации, очистка
// недавних и избранных, сведения о приложении.
//
// Строка настройки — wxl::SettingsCard (SettingsCard Community Toolkit). SettingsExpander и настройка звука
// оригинала (ElementSoundPlayer) ещё не перенесены.

#include "Pages.h"
#include "Shell.h"

#include <wxl/Windows.ApplicationModel.DataTransfer.h>

using namespace wxl;
using namespace wxl::dsl;

namespace {

FrameworkElement sectionHeader(zstring_view label) {
    return TextBlock {label, Margin {1, 30, 0, 6}, styles.TextBlock.BodyStrong};
}

// Версия приложения без пакета — не от чего брать; показывается дата сборки.
std::u16string buildVersion() {
    std::string_view const date = __DATE__;
    return u"Build " + std::u16string(date.begin(), date.end());
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
            gallery::appPop(),
            minWidth = 120,
            isEnabled = !gallery::recentlyVisited().empty(),
            onClick = [](Object const& sender, RoutedEventArgs&) {
                gallery::clearRecentlyVisited();
                sender.try_as<Button>().isEnabled(false);
            },
        },
        Button {
            u"Remove favorites",
            gallery::appPop(),
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
                SettingsCard {
                    header = u"App theme",
                    description = u"Select which app theme to display",
                    headerIcon = FontIcon {glyph = u""},
                    themeChoice(),
                },
                SettingsCard {
                    header = u"Navigation style",
                    headerIcon = FontIcon {glyph = u""},
                    ComboBox {
                        ComboBoxItem {content = u"Left"},
                        ComboBoxItem {content = u"Top"},
                        selectedIndex = 0,
                        onSelectionChanged = [](Object const& sender, SelectionChangedEventArgs&) {
                            gallery::setNavigationOnTop(sender.try_as<ComboBox>().selectedIndex() == 1);
                        },
                    },
                },
                SettingsCard {
                    header = u"Manage samples",
                    description = u"Clear your recent or favorite samples",
                    headerIcon = FontIcon {glyph = u""},
                    manageSamples(),
                },
                SettingsExpander {
                    header = u"Sound",
                    description = u"Controls provide audible feedback",
                    headerIcon = FontIcon {glyph = u"\uEC4F"},
                    ToggleSwitch {
                        onToggled = [](ToggleSwitch const& self, RoutedEventArgs&) {
                            ElementSoundPlayer::state(self.isOn() ? ElementSoundPlayerState::On : ElementSoundPlayerState::Off);
                            if (!self.isOn()) {
                                ElementSoundPlayer::spatialAudioMode(ElementSpatialAudioMode::Off);
                            }
                        },
                    },
                    items[SettingsCard {
                        header = u"Enable Spatial Audio",
                        description = u"Learn more about enabling sounds in your app on the Sound page",
                        isEnabled = ElementSoundPlayer::state() == ElementSoundPlayerState::On,
                        ToggleSwitch {
                            onToggled = [](ToggleSwitch const& self, RoutedEventArgs&) {
                                ElementSoundPlayer::spatialAudioMode(self.isOn() ? ElementSpatialAudioMode::On : ElementSpatialAudioMode::Off);
                            },
                        },
                    }],
                },
                sectionHeader(u"About"),
                SettingsExpander {
                    header = u"WinUI 3 Gallery",
                    description = u"A port of the WinUI 3 Gallery to wxl: the same types, no XAML.",
                    headerIcon = BitmapIcon {uriSource = u"Assets/Tiles/BadgeLogo.png", showAsMonochrome = false},
                    TextBlock {buildVersion(), foreground = brushes.Text.FillColor.Secondary, isTextSelectionEnabled = true},
                    items[
                        SettingsCard {
                            header = u"To clone this repository",
                            isClickEnabled = true,
                            actionIcon = FontIcon {glyph = u"\uE8C8"},
                            TextBlock {
                                u"git clone https://github.com/microsoft/WinUI-Gallery",
                                fontFamily = u"Consolas",
                                foreground = brushes.Text.FillColor.Secondary,
                                isTextSelectionEnabled = true,
                            },
                            onClick = [](auto&&...) {
                                auto package = DataPackage {};
                                package.setText(u"git clone https://github.com/microsoft/WinUI-Gallery");
                                Clipboard::setContent(package);
                            },
                        },
                        SettingsCard {
                            header = u"File a bug or request new sample",
                            actionIcon = FontIcon {glyph = u"\uE8A7"},
                            HyperlinkButton {u"Open issues", navigateUri = u"https://github.com/microsoft/WinUI-Gallery/issues"},
                        },
                        SettingsCard {
                            header = u"Dependencies & references",
                            contentAlignment = SettingsCardContentAlignment::Vertical,
                            StackPanel {
                                HyperlinkButton {u"Windows App SDK 2.4", navigateUri = u"https://aka.ms/windowsappsdk"},
                                HyperlinkButton {u"WinUI 3", navigateUri = u"https://aka.ms/winui"},
                                HyperlinkButton {u"Windows Community Toolkit", navigateUri = u"https://aka.ms/toolkit/windows"},
                                HyperlinkButton {u"Win2D", navigateUri = u"https://github.com/Microsoft/Win2D"},
                            },
                        },
                        SettingsCard {
                            header = u"THIS CODE AND INFORMATION IS PROVIDED \u2018AS IS\u2019 WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR "
                                     u"IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A "
                                     u"PARTICULAR PURPOSE.",
                            contentAlignment = SettingsCardContentAlignment::Vertical,
                            StackPanel {
                                HyperlinkButton {u"Microsoft Services Agreement", navigateUri = u"https://go.microsoft.com/fwlink/?LinkId=822631"},
                                HyperlinkButton {u"Microsoft Privacy Statement", navigateUri = u"https://go.microsoft.com/fwlink/?LinkId=521839"},
                            },
                        }
                    ],
                },
            },
        },
    };
}
