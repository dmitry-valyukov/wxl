// Страница контрола — ItemPage и PageHeader оригинала: предупреждение для
// экспериментального, заголовок с описанием API, документацией и
// избранным, введение и сами примеры.
//
// Введение оригинал даёт статичным TextBlock; здесь это HtmlBlock.

#include "Pages.h"
#include "StringList.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr std::wstring_view winUIBaseUrl =
    L"https://github.com/microsoft/microsoft-ui-xaml/tree/main/controls/dev";

// Подпись-подсказка малого размера над ссылкой или значением во flyout.
FrameworkElement caption(zstring_view text) {
    return TextBlock {
        text,
        styles.TextBlock.Caption,
        foreground = brushes.Text.FillColor.Secondary,
    };
}

// Кнопка-значок «i» с подсказкой, как в заголовке оригинала.
FrameworkElement infoGlyph(zstring_view tip) {
    return FontIcon {
        glyph = u"",
        fontSize = 14,
        foreground = brushes.Accent.TextFillColor.Primary,
        toolTip = tip,
    };
}

FrameworkElement linkRow(zstring_view title, zstring_view uri) {
    return HyperlinkButton {
        hAlign.stretch,
        horizontalContentAlignment = HorizontalAlignment::Left,
        navigateUri = uri,
        toolTip = uri,
        content = TextBlock {title},
    };
}

// Кнопка «API details»: пространство имён и цепочка наследования — здесь
// это обёртки wxl, а не типы WinUI: все они в одном пространстве `wxl` и
// повторяют цепочку наследования своих оригиналов. Панель флайута задана
// шириной: презентер флайута по умолчанию уже, чем цепочка.
FrameworkElement apiDetails(gallery::ControlInfo const& item) {
    if (item.apiNamespace.empty() && item.baseClasses.empty()) {
        return Border {};
    }
    auto panel = StackPanel {width = 420, spacing = 16.0};
    if (!item.apiNamespace.empty()) {
        panel.children().append(StackPanel {
            spacing = 8.0,
            orientation.horizontal,
            caption(u"Namespace"),
            TextBlock {
                u"wxl",
                vAlign.center,
                fontFamily = u"Consolas",
                isTextSelectionEnabled = true,
                styles.TextBlock.Caption,
            },
        });
    }
    if (!item.baseClasses.empty()) {
        // Цепочка наследования — BreadcrumbBar, как в панели оригинала.
        std::vector<std::u16string> chain;
        for (auto const& name : item.baseClasses) {
            chain.emplace_back(name.begin(), name.end());
        }
        panel.children().append(StackPanel {
            caption(u"Inheritance"),
            BreadcrumbBar {itemsSource = stringList(chain)},
        });
    }
    return Button {
        gallery::appPop(),
        Padding {4},
        vAlign.bottom,
        Margin {0, 0, 0, 3},
        styles.Button.Subtle,
        toolTip = u"API namespace and inheritance",
        content = infoGlyph(u"API namespace and inheritance"),
        flyout = Flyout {
            placement = FlyoutPlacementMode::Bottom,
            content = panel,
        },
    };
}

FrameworkElement documentation(gallery::ControlInfo const& item) {
    if (item.docs.empty()) {
        return Border {};
    }
    auto links = StackPanel {};
    for (auto const& doc : item.docs) {
        links.children().append(linkRow(doc.title, doc.uri));
    }
    return DropDownButton {
        gallery::appPop(),
        toolTip = u"Documentation",
        content = StackPanel {
            orientation.horizontal,
            spacing = 8.0,
            FontIcon {glyph = u"", fontSize = 16},
            TextBlock {u"Documentation"},
        },
        flyout = Flyout {
            placement = FlyoutPlacementMode::Bottom,
            content = links,
        },
    };
}

// Ссылка на исходник контрола в репозитории WinUI, как «Control source
// code» оригинала. Ссылок на страницу примера оригинал даёт две (XAML и
// C#); у этого порта своего адреса в сети пока нет.
FrameworkElement sourceLinks(gallery::ControlInfo const& item) {
    if (item.sourcePath.empty()) {
        return Border {};
    }
    auto const uri = std::wstring{winUIBaseUrl} + item.sourcePath;
    return DropDownButton {
        gallery::appPop(),
        toolTip = u"Source code of this control",
        content = StackPanel {
            orientation.horizontal,
            spacing = 8.0,
            FontIcon {glyph = u"", fontSize = 16},
            TextBlock {u"Source"},
        },
        flyout = Flyout {
            placement = FlyoutPlacementMode::Bottom,
            content = StackPanel {
                spacing = 4.0,
                caption(u"Control source code"),
                linkRow(item.title, uri),
            },
        },
    };
}

// Переключатель темы окна: тема ставится корню, как в HelloHere.
FrameworkElement themeButton() {
    return Button {
        gallery::appPop(),
        height = 32,
        Margin {0, 0, 4, 0},
        toolTip = u"Toggle theme",
        content = FontIcon {glyph = u"", fontSize = 16},
        onClick = [](Object const& sender, RoutedEventArgs&) {
            auto const button = sender.try_as<Button>();
            if (auto const root = button.xamlRoot().content().try_as<FrameworkElement>()) {
                auto const light = root.actualTheme() == ElementTheme::Light;
                root.requestedTheme(light ? ElementTheme::Dark : ElementTheme::Light);
            }
        },
    };
}

FrameworkElement favoriteButton(std::wstring id) {
    bool const on = gallery::isFavorite(id);
    return ToggleButton {
        gallery::appPop(),
        height = 32,
        Margin {4, 0, 0, 0},
        isChecked = on,
        toolTip = on ? u"Remove from favorites" : u"Add to favorites",
        content = FontIcon {glyph = on ? u"" : u"", fontSize = 16},
        onClick = [id](Object const& sender, RoutedEventArgs&) {
            auto const button = sender.try_as<ToggleButton>();
            bool const checked = button.isChecked().value_or(false);
            gallery::setFavorite(id, checked);
            button.content(FontIcon {glyph = checked ? u"" : u"", fontSize = 16});
            button.toolTip(checked ? u"Remove from favorites" : u"Add to favorites");
        },
    };
}

FrameworkElement pageHeader(gallery::ControlInfo const& item) {
    return Grid {
        rowDefinitions = u"auto,auto",
        Margin {36, 24, 36, 0},
        StackPanel {
            orientation.horizontal,
            spacing = 4.0,
            TextBlock {
                item.title,
                styles.TextBlock.Title,
                textTrimming = TextTrimming::CharacterEllipsis,
                textWrapping.noWrap,
            },
            apiDetails(item),
        },
        Grid {
            row = 1,
            Margin {0, 12},
            StackPanel {
                orientation.horizontal,
                spacing = 4.0,
                documentation(item),
                sourceLinks(item),
            },
            StackPanel {
                hAlign.right,
                orientation.horizontal,
                themeButton(),
                AppBarSeparator {},
                favoriteButton(item.uniqueId),
            },
        },
    };
}

}  // namespace

wxl::FrameworkElement gallery::itemPage(ControlInfo const& item) {
    auto const description = gallery::htmlEscape(item.description);

    auto body = Grid {
        rowDefinitions = u"auto,*",
        Padding {36, 0, 36, 36},
    };
    if (!description.empty()) {
        body.children().append(HtmlBlock {
            maxWidth = 1064,
            hAlign.left,
            Margin {0, 4, 24, 0},
            isTextSelectionEnabled = true,
            description,
        });
    }
    if (auto const page = gallery::pageFor(item.uniqueId)) {
        body.children().append(Border {row = 1, page()});
    }

    auto root = Grid {rowDefinitions = u"auto,auto,*"};
    if (item.isExperimental) {
        root.children().append(InfoBar {
            isClosable = false,
            isOpen = true,
            severity = InfoBarSeverity::Warning,
            title = u"Experimental",
            message = u"This sample uses an experimental Windows App SDK API that may change before release.",
        });
    }
    root.children().append(Border {row = 1, pageHeader(item)});
    root.children().append(ScrollViewer {row = 2, content = body});
    return root;
}
