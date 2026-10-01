// Оболочка: окно с TitleBar и NavigationView, история переходов и то, что
// оригинал держит в SettingsHelper.
//
// Frame оригинала — один `Border`: `Frame.Navigate` берёт тип страницы, а
// страницы здесь — функции, строящие элемент, так что переход это
// `host.child(страница)` плюс стеки «назад» и «вперёд». Пункт навигации
// узнаётся по `name`: обработчик выбора получает элемент, а не запись о нём,
// и хранить рядом второй список нечем и незачем.

#include "Shell.h"

#include "Pages.h"

#include <algorithm>
#include <memory>

#include "CompositionWindow.h"
#include "generated/Microsoft.UI.Windowing.h"

using namespace wxl;
using namespace wxl::dsl;

namespace gallery {

namespace {

// ---- То, что оригинал держит в SettingsHelper ------------------------------

std::vector<std::wstring> recent;
std::vector<std::wstring> favorite;

constexpr std::size_t maxRecentlyVisited = 7;

// ---- Имена пунктов панели --------------------------------------------------

constexpr std::wstring_view homeName = L"home";
constexpr std::wstring_view allControlsName = L"all";

std::wstring groupName(std::wstring_view id) {
    return L"group:" + std::wstring{id};
}

std::wstring itemName(std::wstring_view id) {
    return L"item:" + std::wstring{id};
}

// Разделы, которые оригинал пишет в MainWindow.xaml, а не берёт из каталога:
// у них свои значки и подписи.
struct SpecialItem {
    std::wstring_view id;
    zstring_view content;
    zstring_view glyph;  // пусто — без значка
};

struct SpecialSection {
    zstring_view content;
    zstring_view glyph;
    std::vector<SpecialItem> items;
};

std::vector<SpecialSection> const& specialSections() {
    static std::vector<SpecialSection> const sections{
        {L"Fundamentals",
         L"",
         {{L"XamlResources", L"Resources", L""},
          {L"XamlStyles", L"Styles", L""},
          {L"Binding", L"Binding", L""},
          {L"Templates", L"Templates", L""},
          {L"CustomUserControls", L"Custom & User Controls", L""},
          {L"CustomXamlConditionals", L"XAML Conditions", L""},
          {L"ScratchPad", L"Scratch Pad", L""}}},
        {L"Design",
         L"",
         {{L"Color", L"Color", L""},
          {L"Geometry", L"Geometry", L""},
          {L"Iconography", L"Iconography", L""},
          {L"Spacing", L"Spacing", L""},
          {L"Typography", L"Typography", L""}}},
        {L"Accessibility",
         L"",
         {{L"AccessibilityScreenReader", L"Screen reader support", L""},
          {L"AccessibilityKeyboard", L"Keyboard support", L""},
          {L"AccessibilityColorContrast", L"Color contrast", L""}}},
    };
    return sections;
}

// Подпись пункта контрола: название и, у экспериментального, плашка
// (NavigationItemContentTemplate оригинала).
FrameworkElement itemContent(ControlInfo const& item) {
    auto content = Grid {
        columnDefinitions = u"*,auto",
        columnSpacing = 8.0,
        hAlign.stretch,
        TextBlock {
            item.title,
            vAlign.center,
            textTrimming = TextTrimming::CharacterEllipsis,
        },
    };
    if (item.isExperimental) {
        content.children().append(Border {
            column = 1,
            Padding {6, 2},
            vAlign.center,
            CornerRadius {4},
            background = brushes.SystemFillColor.CautionBackground,
            TextBlock {
                u"Experimental",
                styles.TextBlock.Caption,
                foreground = brushes.SystemFillColor.Caution,
            },
        });
    }
    return content;
}

// ---- Сама оболочка ---------------------------------------------------------


void onSelected(NavigationView const& sender, bool settingsSelected);
void goBack();
void togglePane();
void submitQuery(std::wstring query);

struct Shell {
    Border host;
    NavigationView navigation {
        row = 1,
        isBackButtonVisible = NavigationViewBackButtonVisible::Collapsed,
        isPaneToggleButtonVisible = false,
        content = host,
        onSelectionChanged = [](Object const& sender, NavigationViewSelectionChangedEventArgs& args) {
            onSelected(sender.try_as<NavigationView>(), args.isSettingsSelected());
        },
    };
    TitleBar titleBar {
        title = u"WinUI 3 Gallery",
        isBackButtonVisible = false,
        isPaneToggleButtonVisible = true,
        iconSource = ImageIconSource {imageSource = u"Assets/Tiles/GalleryIcon.ico"},
        onBackRequested = [] { goBack(); },
        onPaneToggleRequested = [] { togglePane(); },
        content = AutoSuggestBox {
            maxWidth = 580.0,
            hAlign.stretch,
            vAlign.center,
            placeholderText = u"Search controls and samples...",
            queryIcon = SymbolIcon {symbol = FluentSymbol::Search},
            onQuerySubmitted = [](Object const&, AutoSuggestBoxQuerySubmittedEventArgs& args) {
                submitQuery(gallery::wide(args.queryText()));
            },
        },
    };
    // Пункты панели по порядку и индекс родителя каждого (-1 — верхний
    // уровень): по ним `select` находит элемент и раскрывает его группу.
    std::vector<NavigationViewItem> items;
    std::vector<int> parents;
    std::vector<Destination> back;
    std::vector<Destination> forward;
    // Окна, открытые примерами (см. trackWindow).
    struct Tracked {
        Window window;
        std::shared_ptr<void> state;
    };
    std::vector<Tracked> windows;
    struct TrackedComposition {
        CompositionWindow window;
        std::shared_ptr<void> state;
    };
    std::vector<TrackedComposition> compositionWindows;
    // Главное окно: владелец модальных окон примеров (см. mainWindow).
    std::shared_ptr<Window> main;
    // Модели показанной страницы (см. holdModel).
    std::vector<std::shared_ptr<void>> models;
    Destination current;
    bool hasCurrent = false;
    // Выбор в панели ставит сам `select`; его событие не должно вести
    // на страницу второй раз.
    bool selecting = false;
};

std::shared_ptr<Shell> shell;

FrameworkElement buildPage(Destination const& destination) {
    auto const& all = catalog();
    switch (destination.place) {
    case Place::Home:
        return homePage();
    case Place::AllControls:
        return allControlsPage();
    case Place::Section:
        if (auto const* group = all.group(destination.id)) {
            return sectionPage(*group);
        }
        return homePage();
    case Place::Item:
        if (auto const* item = all.find(destination.id)) {
            return itemPage(*item);
        }
        return homePage();
    case Place::Search:
        return searchResultsPage(destination.id);
    case Place::Settings:
        return settingsPage();
    }
    return homePage();
}

// Имя пункта, которому соответствует переход, или пусто.
std::wstring nameOf(Destination const& destination) {
    switch (destination.place) {
    case Place::Home:
        return std::wstring{homeName};
    case Place::AllControls:
        return std::wstring{allControlsName};
    case Place::Section:
        return groupName(destination.id);
    case Place::Item:
        return itemName(destination.id);
    default:
        return {};
    }
}

// EnsureNavigationSelection оригинала: выбрать пункт, раскрыв его группу.
void select(Destination const& destination) {
    auto const name = nameOf(destination);
    if (name.empty()) {
        return;
    }
    auto& s = *shell;
    for (std::size_t i = 0; i < s.items.size(); ++i) {
        if (gallery::wide(s.items[i].name()) == name) {
            s.selecting = true;
            if (s.parents[i] >= 0) {
                s.items[static_cast<std::size_t>(s.parents[i])].isExpanded(true);
            }
            s.navigation.selectedItem(s.items[i]);
            s.selecting = false;
            return;
        }
    }
}

void show(Destination destination, bool record) {
    auto& s = *shell;
    if (record && s.hasCurrent) {
        s.back.push_back(s.current);
        s.forward.clear();
    }
    s.current = destination;
    s.hasCurrent = true;

    if (destination.place == Place::Item) {
        std::erase(recent, destination.id);
        recent.insert(recent.begin(), destination.id);
        if (recent.size() > maxRecentlyVisited) {
            recent.resize(maxRecentlyVisited);
        }
    }

    // Модели прежней страницы отпускаются, когда новая уже на месте.
    auto const previous = std::move(s.models);
    s.models.clear();
    s.host.child(buildPage(destination));
    s.titleBar.isBackButtonVisible(!s.back.empty());
    select(destination);
}

void goBack() {
    auto& s = *shell;
    if (s.back.empty()) {
        return;
    }
    s.forward.push_back(s.current);
    auto const destination = s.back.back();
    s.back.pop_back();
    show(destination, false);
}

void togglePane() {
    auto& s = *shell;
    s.navigation.isPaneOpen(!s.navigation.isPaneOpen());
}

void submitQuery(std::wstring query) {
    if (!query.empty()) {
        show({Place::Search, std::move(query)}, true);
    }
}

// Переход по выбору пункта панели (OnNavigationViewSelectionChanged).
void onSelected(NavigationView const& sender, bool settingsSelected) {
    auto& s = *shell;
    if (s.selecting) {
        return;
    }
    if (settingsSelected) {
        if (!s.hasCurrent || s.current.place != Place::Settings) {
            show({Place::Settings, {}}, true);
        }
        return;
    }
    auto const selected = sender.selectedItem().try_as<FrameworkElement>();
    if (!selected) {
        return;
    }
    auto const name = gallery::wide(selected.name());
    if (name == homeName) {
        show({Place::Home, {}}, true);
    } else if (name == allControlsName) {
        show({Place::AllControls, {}}, true);
    } else if (name.starts_with(L"group:")) {
        show({Place::Section, name.substr(6)}, true);
    } else if (name.starts_with(L"item:")) {
        show({Place::Item, name.substr(5)}, true);
    }
}

// Записывает пункт в списки оболочки и отдаёт его же.
NavigationViewItem addItem(Shell& s, std::wstring name, int parent, NavigationViewItem item) {
    item.name(name);
    s.items.push_back(item);
    s.parents.push_back(parent);
    return item;
}

int lastIndex(Shell const& s) {
    return static_cast<int>(s.items.size()) - 1;
}

void buildMenu(Shell& s) {
    auto const& all = catalog();

    s.navigation.menuItems().append(addItem(s, std::wstring{homeName}, -1,
                                            NavigationViewItem {
                                                content = u"Home",
                                                icon = SymbolIcon {symbol = FluentSymbol::Home},
                                            }));

    for (auto const& section : specialSections()) {
        auto group = NavigationViewItem {
            content = section.content,
            selectsOnInvoked = false,
            icon = FontIcon {glyph = section.glyph},
        };
        s.navigation.menuItems().append(group);
        for (auto const& entry : section.items) {
            auto row = NavigationViewItem {content = entry.content};
            if (!entry.glyph.empty()) {
                row.icon(FontIcon {glyph = entry.glyph});
            }
            row.isEnabled(all.find(entry.id) && pageFor(entry.id));
            group.menuItems().append(addItem(s, itemName(entry.id), -1, row));
        }
    }

    s.navigation.menuItems().append(NavigationViewItemHeader {content = u"Controls"});

    s.navigation.menuItems().append(addItem(s, std::wstring{allControlsName}, -1,
                                            NavigationViewItem {
                                                content = u"All",
                                                icon = FontIcon {glyph = u""},
                                            }));

    std::vector<ControlGroup const*> ordered;
    for (auto const& group : all.groups) {
        if (!group.isSpecialSection) {
            ordered.push_back(&group);
        }
    }
    std::ranges::sort(ordered, [](ControlGroup const* a, ControlGroup const* b) {
        return a->title < b->title;
    });

    for (auto const* group : ordered) {
        auto row = NavigationViewItem {content = group->title};
        if (!group->iconGlyph.empty()) {
            row.icon(FontIcon {glyph = group->iconGlyph});
        }
        addItem(s, groupName(group->uniqueId), -1, row);
        s.navigation.menuItems().append(row);
        int const parent = lastIndex(s);
        for (auto const& item : group->items) {
            auto child = NavigationViewItem {content = itemContent(item)};
            child.horizontalContentAlignment(HorizontalAlignment::Stretch);
            child.isEnabled(pageFor(item.uniqueId) != nullptr);
            row.menuItems().append(addItem(s, itemName(item.uniqueId), parent, child));
        }
    }
}

}  // namespace

// ---- Настройки -------------------------------------------------------------

std::vector<std::wstring> const& recentlyVisited() {
    return recent;
}

void clearRecentlyVisited() {
    recent.clear();
}

std::vector<std::wstring> const& favorites() {
    return favorite;
}

bool isFavorite(std::wstring_view id) {
    return std::ranges::find(favorite, id) != favorite.end();
}

void setFavorite(std::wstring_view id, bool on) {
    std::erase(favorite, id);
    if (on) {
        favorite.emplace_back(id);
    }
}

void clearFavorites() {
    favorite.clear();
}

// ---- Окно и переходы -------------------------------------------------------

void navigate(Destination destination) {
    show(std::move(destination), true);
}

void holdModel(std::shared_ptr<void> model) {
    shell->models.push_back(std::move(model));
}

Window createMainWindow() {
    shell = std::make_shared<Shell>();
    auto& s = *shell;
    buildMenu(s);

    Window window {
        title = u"WinUI 3 Gallery",
        minSize = {640, 500},
        systemBackdrop = MicaBackdrop {},
        extendsContentIntoTitleBar = true,
        content = Grid {
            rowDefinitions = u"auto,*",
            s.titleBar,
            s.navigation,
        },
    };
    window.setTitleBar(s.titleBar);
    window.appWindow().resize({1280, 800});
    s.main = std::make_shared<Window>(window);

    show({Place::Home, {}}, true);

    // Для проверки страницы без ввода: GALLERY_PAGE=<UniqueId> открывает её сразу.
    if (wchar_t const* const page = _wgetenv(L"GALLERY_PAGE")) {
        show({Place::Item, page}, true);
    }
    return window;
}

Window const& mainWindow() {
    return *shell->main;
}

void trackWindow(Window const& window, std::shared_ptr<void> state) {
    shell->windows.push_back({window, std::move(state)});
    window.add_onClosed([](Object const& sender, auto&) {
        if (shell) {
            std::erase_if(shell->windows, [&](Shell::Tracked const& each) { return each.window.is_same_object(sender); });
        }
    });
}

void trackWindow(CompositionWindow const& window, std::shared_ptr<void> state) {
    shell->compositionWindows.push_back({window, std::move(state)});
    window.add_onClosed([handle = window.handle()](auto&&...) {
        if (shell) {
            std::erase_if(shell->compositionWindows,
                          [&](Shell::TrackedComposition const& each) { return each.window.handle() == handle; });
        }
    });
}

void destroyMainWindow() {
    for (auto const& tracked : std::vector<Shell::TrackedComposition>(shell->compositionWindows)) {
        tracked.window.close();
    }
    // Закрытие убирает окно из списка, поэтому закрываем копию.
    for (auto const& tracked : std::vector<Shell::Tracked>(shell->windows)) {
        tracked.window.close();
    }
    shell.reset();
}

}  // namespace gallery
