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
#include "StringList.h"

#include <algorithm>
#include <memory>

#include "CompositionWindow.h"
#include <wxl/Microsoft.Windows.AppNotifications.h>
#include <wxl/Microsoft.UI.Windowing.h>

import wxl.async;
import wxl.core;

using namespace wxl;
using namespace wxl::dsl;

namespace gallery {

namespace {

// ---- То, что оригинал держит в SettingsHelper ------------------------------

std::vector<std::wstring> recent;
std::vector<std::wstring> favorite;

constexpr std::size_t maxRecentlyVisited = 7;

// Недавние, избранные и размер окна живут между запусками (SettingsHelper оригинала пишет их в ApplicationData, а у
// приложения без пакета её нет): файл в %LOCALAPPDATA%\wxl\Gallery. Он крошечный, и пишется при каждой перемене,
// чтобы закрытие процесса ничего не теряло. Названия контролов — ASCII.
struct WindowSize {
    int width = 1280;
    int height = 800;
};
WindowSize savedSize;

std::filesystem::path stateFile() {
    wchar_t const* const base = _wgetenv(L"LOCALAPPDATA");
    return base ? std::filesystem::path{base} / L"wxl" / L"Gallery" / L"state.txt" : std::filesystem::path{};
}

void loadState() {
    auto const file = stateFile();
    if (file.empty()) {
        return;
    }
    std::ifstream input{file};
    std::string kind;
    while (input >> kind) {
        if (kind == "size") {
            WindowSize size;
            if (input >> size.width >> size.height && size.width >= 640 && size.height >= 500) {
                savedSize = size;
            }
        } else {
            std::string id;
            if (input >> id) {
                (kind == "recent" ? recent : favorite).emplace_back(id.begin(), id.end());
            }
        }
    }
    if (recent.size() > maxRecentlyVisited) {
        recent.resize(maxRecentlyVisited);
    }
}

// Файл пишется целиком с заменой операцией wxl и не в потоке окна: прежде
// ofstream писал его прямо из обработчиков. Два сохранения подряд не
// обгоняют друг друга по смыслу — каждое несёт полное состояние, а за
// одним переходом второй следует не раньше, чем через человеческую паузу.
// Сохранение, которое не удалось, молчит: без этого файла Gallery всего
// лишь забудет недавние, а окно с ошибкой при каждом переходе было бы хуже.
async::detached_task saveState() {
    auto const file = stateFile();
    if (file.empty()) {
        co_return;
    }
    std::string text;
    // Имена контролов — ASCII из каталога, а прочитанные из файла расширены из
    // байтов по одному (loadState), так что суррогатов в них нет и проверять
    // нечего. Сужение — через UTF-8, а не поэлементным копированием: то молча
    // обрезает wchar_t до char.
    auto const narrow = [](std::wstring const& id) {
        return std::string{core::unicode::assume_valid(std::wstring_view{id}).to_utf8().chars()};
    };
    for (auto const& id : recent) {
        text += "recent " + narrow(id) + '\n';
    }
    for (auto const& id : favorite) {
        text += "favorite " + narrow(id) + '\n';
    }
    text += std::format("size {} {}\n", savedSize.width, savedSize.height);
    try {
        co_await async::async_file::write_all(core::path(std::wstring_view(file.native())), std::move(text));
    } catch (async::system_exception const&) {
    }
}

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
          {L"BoundCollection", L"Bound collection", L""},
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
void goForward();
void togglePane();
void submitQuery(std::wstring query);
void focusSearch();

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
    // Поле поиска: Ctrl+F переводит на него фокус.
    AutoSuggestBox search {
        minWidth = 320.0,
        hAlign.stretch,
        vAlign.center,
        // Содержимое TitleBar лежит в PART_ContentPresenter с выравниванием по центру, и поле получает ширину по тексту:
        // растягиваем его на всю область содержимого, ширина которой от самого поля не зависит.
        onLoaded = [](Object const& sender, auto&&...) {
            auto const box = sender.try_as<AutoSuggestBox>();
            auto const area = VisualTreeHelper::getParent(VisualTreeHelper::getParent(box)).try_as<FrameworkElement>();
            if (!area) {
                return;
            }
            box.width(area.actualWidth());
            area.add_onSizeChanged([box](Object const& area, auto&&...) { box.width(area.try_as<FrameworkElement>().actualWidth()); });
        },
        placeholderText = u"Search controls and samples...",
        queryIcon = SymbolIcon {symbol = FluentSymbol::Search},
        onTextChanged = [](AutoSuggestBox const& self, AutoSuggestBoxTextChangedEventArgs& args) {
            // Подсказки даёт только ввод пользователя; выбранная подсказка сама вписывается в текст.
            if (args.reason() != AutoSuggestionBoxTextChangeReason::UserInput) {
                return;
            }
            auto found = gallery::controlTitles(self.text());
            if (found.empty()) {
                found.emplace_back(u"No results found");
            }
            self.itemsSource(stringList(found));
        },
        onSuggestionChosen = [](AutoSuggestBox const& self, AutoSuggestBoxSuggestionChosenEventArgs& args) {
            auto const title = stringOf(args.selectedItem());
            if (title != u"No results found") {
                self.text(title);
            }
        },
        onQuerySubmitted = [](Object const&, AutoSuggestBoxQuerySubmittedEventArgs& args) {
            // Выбранная подсказка ведёт прямо на страницу контрола, набранный текст — на результаты поиска.
            if (args.chosenSuggestion()) {
                auto const title = stringOf(args.chosenSuggestion());
                if (auto const* control = gallery::controlByTitle(title)) {
                    navigate({Place::Item, control->uniqueId});
                }
                return;
            }
            submitQuery(gallery::wide(args.queryText()));
        },
    };
    TitleBar titleBar {
        title = u"WXL Gallery",
        isBackButtonVisible = false,
        isPaneToggleButtonVisible = true,
        iconSource = ImageIconSource {imageSource = u"Assets/Tiles/GalleryIcon.ico"},
        onBackRequested = [] { goBack(); },
        onPaneToggleRequested = [] { togglePane(); },
        content = search,
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
    // Место показанной страницы (см. pageSize).
    std::shared_ptr<PageSize> size;
    // Модели показанной страницы (см. holdModel).
    std::vector<std::shared_ptr<void>> models;
    Destination current;
    bool hasCurrent = false;
    // Выбор в панели ставит сам `select`; его событие не должно вести
    // на страницу второй раз.
    bool selecting = false;
};

std::shared_ptr<Shell> shell;

void focusSearch() {
    shell->search.focus(FocusState::Programmatic);
}

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
        saveState();
    }

    // Модели прежней страницы отпускаются, когда новая уже на месте.
    auto const previous = std::move(s.models);
    s.models.clear();
    // Место заводится до страницы: её свойства привязываются к его полям, пока она строится.
    s.size = hold<PageSize>();
    s.host.child(LayoutPanel {
        layout = AvailableSizeLayout {availableWidth = BindInput {s.size->width}},
        buildPage(destination),
    });
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

// Кнопка мыши «вперёд»: возвращает на страницу, с которой ушли назад.
void goForward() {
    auto& s = *shell;
    if (s.forward.empty()) {
        return;
    }
    s.back.push_back(s.current);
    auto const destination = s.forward.back();
    s.forward.pop_back();
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
        // The notifications of the application are shown by the shell once it knows the application.
    if (AppNotificationManager::isSupported()) {
        try {
            AppNotificationManager::default_().register_();
        } catch (...) {
            // An application the shell cannot register shows no notifications; the page says nothing else.
        }
    }

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
                                                automationId = u"Home",
                                                content = u"Home",
                                                icon = SymbolIcon {symbol = FluentSymbol::Home},
                                            }));

    for (auto const& section : specialSections()) {
        auto group = NavigationViewItem {
            automationId = std::u16string(section.content.begin(), section.content.end()) + u"Item",
            content = section.content,
            selectsOnInvoked = false,
            icon = FontIcon {glyph = section.glyph},
        };
        s.navigation.menuItems().append(group);
        for (auto const& entry : section.items) {
            auto row = NavigationViewItem {automationId = std::wstring{entry.id}, content = entry.content};
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
            auto child = NavigationViewItem {automationId = item.uniqueId, content = itemContent(item)};
            child.horizontalContentAlignment(HorizontalAlignment::Stretch);
            child.icon(ImageIcon {source = gallery::assetPath(item.imagePath)});
            child.isEnabled(pageFor(item.uniqueId) != nullptr);
            row.menuItems().append(addItem(s, itemName(item.uniqueId), parent, child));
        }
    }
}

}  // namespace

// ---- Настройки -------------------------------------------------------------


void setNavigationOnTop(bool top) {
    if (shell) {
        shell->navigation.paneDisplayMode(top ? NavigationViewPaneDisplayMode::Top : NavigationViewPaneDisplayMode::Auto);
        // Страница, что открыта, остаётся с раскладкой прежнего вида панели: её строят заново.
        if (shell->hasCurrent) {
            show(shell->current, false);
        }
    }
}

bool navigationOnTop() {
    return shell && shell->navigation.paneDisplayMode() == NavigationViewPaneDisplayMode::Top;
}

std::vector<std::wstring> const& recentlyVisited() {
    return recent;
}

void clearRecentlyVisited() {
    recent.clear();
    saveState();
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
    saveState();
}

void clearFavorites() {
    favorite.clear();
    saveState();
}

// ---- Окно и переходы -------------------------------------------------------

PageSize::PageSize() {
    wide.follow(width, [](double room) { return room >= 641; });
    optionsBeside.follow(width, [](double room) { return room >= 740; });
}

PageSize& pageSize() {
    return *shell->size;
}

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
        title = u"WXL Gallery",
        minSize = {640, 500},
        systemBackdrop = MicaBackdrop {},
        extendsContentIntoTitleBar = true,
        content = Grid {
            rowDefinitions = u"auto,*",
            s.titleBar,
            s.navigation,
            // Боковые кнопки мыши — «назад» и «вперёд», как у оригинала.
            onPointerPressed = [](Object const& sender, PointerRoutedEventArgs& args) {
                auto const properties = args.getCurrentPoint(sender.try_as<UIElement>()).properties();
                if (properties.isXButton1Pressed()) {
                    goBack();
                } else if (properties.isXButton2Pressed()) {
                    goForward();
                }
            },
            keyboardAccelerators[KeyboardAccelerator {
                key = VirtualKey::F,
                modifiers = VirtualKeyModifiers::Control,
                onInvoked = [](Object const&, KeyboardAcceleratorInvokedEventArgs& args) {
                    focusSearch();
                    args.handled(true);
                },
            }],
        },
    };
    window.appWindow().titleBar().preferredHeightOption(TitleBarHeightOption::Tall);
    window.setTitleBar(s.titleBar);
    window.appWindow().setIcon(u"Assets/Tiles/GalleryIcon.ico");
    loadState();
    window.appWindow().resize({savedSize.width, savedSize.height});
    window.add_onClosed([](auto&&...) {
        if (shell && shell->main) {
            auto const size = shell->main->appWindow().size();
            savedSize = {size.width, size.height};
            saveState();
        }
    });
    s.main = std::make_shared<Window>(window);

    // The notifications of the application are shown by the shell once it knows the application.
    if (AppNotificationManager::isSupported()) {
        try {
            AppNotificationManager::default_().register_();
        } catch (...) {
            // An application the shell cannot register shows no notifications; the page says nothing else.
        }
    }

    show({Place::Home, {}}, true);

    // Для проверки страницы без ввода: GALLERY_PAGE=<UniqueId> открывает её сразу (Settings — страницу настроек),
    // GALLERY_TOP=1 ставит панель навигации сверху.
    if (wchar_t const* const top = _wgetenv(L"GALLERY_TOP"); top && *top) {
        setNavigationOnTop(true);
    }
    if (wchar_t const* const page = _wgetenv(L"GALLERY_PAGE"); page && *page) {
        std::wstring_view const name {page};
        show({name == L"Settings" ? Place::Settings : name == L"AllControls" ? Place::AllControls : Place::Item, page}, true);
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
    if (AppNotificationManager::isSupported()) {
        try {
            AppNotificationManager::default_().unregister();
        } catch (...) {
        }
    }
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
