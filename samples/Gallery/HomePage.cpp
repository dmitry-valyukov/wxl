// Домашняя страница — HomePage и HomePageHeader оригинала: шапка с плитками
// ссылок, ниже переключатель «Recent» / «Favorites» с плитками контролов.
//
// Шапка упрощена: фон и затухание картинки к низу (OpacityMaskView
// оригинала) не воспроизведены.

#include "Pages.h"
#include "Shell.h"

#include <algorithm>
#include <memory>

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Плитка ссылки в шапке (Tile оригинала).
FrameworkElement headerTile(std::wstring_view title, std::wstring_view description,
                            std::wstring_view link, FrameworkElement const& source) {
    return HyperlinkButton {
        width = 232,
        height = 172,
        hAlign.stretch,
        vAlign.stretch,
        horizontalContentAlignment = HorizontalAlignment::Stretch,
        verticalContentAlignment = VerticalAlignment::Stretch,
        navigateUri = link,
        Padding {0},
        content = Border {
            Padding {24},
            background = brushes.Control.FillColor.Default,
            borderBrush = brushes.Card.StrokeColorDefault,
            BorderThickness {1},
            CornerRadius {8},
            StackPanel {
                spacing = 12.0,
                Border {height = 36, hAlign.left, source},
                TextBlock {title, styles.TextBlock.BodyStrong, foreground = brushes.Text.FillColor.Primary},
                TextBlock {
                    description,
                    styles.TextBlock.Caption,
                    foreground = brushes.Text.FillColor.Secondary,
                    textWrapping.wrap,
                },
            },
        },
    };
}

FrameworkElement picture(std::u16string_view path) {
    return Image {source = path, height = 36};
}

FrameworkElement heroHeader() {
    return Grid {
        rowDefinitions = u"auto,auto,*",
        height = 400,
        Border {
            rowSpan = 3,
            vAlign.stretch,
            background = brushes.SolidBackgroundFillColor.Base,
            Image {
                source = u"Assets/GalleryHeaderImage.png",
                stretch = Stretch::UniformToFill,
                opacity = 0.8,
            },
        },
        StackPanel {
            Margin {36, 48, 0, 0},
            vAlign.center,
            TextBlock {u"Windows App SDK 2.4 · wxl", fontSize = 18},
            TextBlock {u"WinUI 3 Gallery", styles.TextBlock.TitleLarge},
        },
        ScrollViewer {
            row = 2,
            Margin {0, 56, 0, 0},
            horizontalScrollBarVisibility = ScrollBarVisibility::Auto,
            verticalScrollBarVisibility = ScrollBarVisibility::Disabled,
            verticalScrollMode = ScrollMode::Disabled,
            content = StackPanel {
                orientation.horizontal,
                spacing = 12.0,
                Margin {36, 0},
                headerTile(L"Getting started",
                           L"Get started with WinUI and explore detailed documentation.",
                           L"https://aka.ms/winui-getstarted", picture(u"Assets/HomeHeaderTiles/Header-WinUI.png")),
                headerTile(L"Design", L"Guidelines and toolkits for creating stunning WinUI experiences.",
                           L"https://learn.microsoft.com/windows/apps/design/",
                           picture(u"Assets/HomeHeaderTiles/Header-WindowsDesign.png")),
                headerTile(L"WinUI on GitHub", L"Explore the WinUI source code and repository.",
                           L"https://github.com/microsoft/microsoft-ui-xaml",
                           FontIcon {glyph = u"", fontSize = 24}),
                headerTile(L"Community Toolkit",
                           L"A collection of helper functions, controls, and app services.",
                           L"https://apps.microsoft.com/store/detail/windows-community-toolkit-sample-app/9NBLGGH4TLCQ",
                           picture(u"Assets/HomeHeaderTiles/Header-Toolkit.png")),
                headerTile(L"Code samples", L"Find samples that demonstrate specific tasks, features, and APIs.",
                           L"https://learn.microsoft.com/windows/apps/get-started/samples",
                           FontIcon {glyph = u"", fontSize = 24}),
                headerTile(L"Partner Center", L"Upload your app to the Store.",
                           L"https://developer.microsoft.com/windows/",
                           picture(u"Assets/HomeHeaderTiles/Header-Store.light.png")),
            },
        },
    };
}

FrameworkElement heading(std::u16string_view label, Thickness space) {
    return TextBlock {label, margin = space, fontSize = 16, styles.TextBlock.BodyStrong};
}

// Контролы по идентификаторам, которые ещё есть в каталоге.
std::vector<gallery::ControlInfo const*> byIds(std::vector<std::wstring> const& ids) {
    std::vector<gallery::ControlInfo const*> items;
    for (auto const& id : ids) {
        if (auto const* item = gallery::catalog().find(id)) {
            items.push_back(item);
        }
    }
    return items;
}

}  // namespace

wxl::FrameworkElement gallery::homePage() {
    std::vector<ControlInfo const*> added;
    for (auto const& group : catalog().groups) {
        for (auto const& item : group.items) {
            if (item.isNew || item.isUpdated) {
                added.push_back(&item);
            }
        }
    }
    std::ranges::sort(added, [](ControlInfo const* a, ControlInfo const* b) { return a->title < b->title; });

    auto const visited = byIds(recentlyVisited());
    auto const liked = byIds(favorites());

    auto recentPanel = StackPanel {spacing = 12.0};
    if (!visited.empty()) {
        recentPanel.children().append(heading(u"Recently visited", Thickness {0}));
        recentPanel.children().append(tileGrid(visited, Thickness {0}));
    }
    recentPanel.children().append(heading(u"Recently added or updated", Thickness {0, 12, 0, 0}));
    recentPanel.children().append(tileGrid(added, Thickness {0}));

    auto favoritePanel = StackPanel {};
    if (liked.empty()) {
        favoritePanel.children().append(StackPanel {
            Margin {24, 36},
            Image {height = 36, source = u"Assets/ControlImages/RatingControl.png"},
            TextBlock {u"No favorites yet", Margin {0, 8}, hAlign.center, styles.TextBlock.BodyStrong},
            TextBlock {
                u"Favorite samples by clicking the star icon on the sample page.",
                hAlign.center,
                textAlignment.center,
                foreground = brushes.Text.FillColor.Secondary,
            },
        });
    } else {
        favoritePanel.children().append(tileGrid(liked, Thickness {0}));
    }

    auto body = Border {row = 2, Margin {36, 0, 36, 36}, recentPanel};

    auto bar = SelectorBar {
        row = 1,
        Margin {36, 24, 0, 16},
        hAlign.center,
        onSelectionChanged = [body, recentPanel, favoritePanel](Object const& sender, SelectorBarSelectionChangedEventArgs&) {
            auto const selected = sender.try_as<SelectorBar>().selectedItem();
            if (selected && gallery::wide(selected.text()) == L"Favorites") {
                body.child(favoritePanel);
            } else {
                body.child(recentPanel);
            }
        },
    };
    auto recentItem = SelectorBarItem {text = u"Recent", icon = SymbolIcon {symbol = FluentSymbol::Clock}};
    bar.items().append(recentItem);
    bar.items().append(SelectorBarItem {text = u"Favorites", icon = SymbolIcon {symbol = FluentSymbol::FavoriteStar}});
    bar.selectedItem(recentItem);

    return ScrollViewer {
        verticalScrollBarVisibility = ScrollBarVisibility::Auto,
        content = Grid {
            rowDefinitions = u"auto,auto,*",
            heroHeader(),
            bar,
            body,
        },
    };
}
