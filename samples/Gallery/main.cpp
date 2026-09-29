// WinUI 3 Gallery без XAML: окно с NavigationView слева и страницей контрола
// справа — MainWindow оригинала.
//
// `Frame` оригинала с его навигацией здесь не нужен: NavigationView держит
// одну страницу, и переключение — подмена `content`. Что такое страница,
// сказано в Pages.h.

#include "Pages.h"

#include <memory>
#include <vector>

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Пункт левой панели и страница, которую он показывает.
struct Entry {
    NavigationViewItem item;
    FrameworkElement (*page)();
};

FrameworkElement homePage() {
    return HtmlBlock {
        isTextSelectionEnabled = true,
        Margin {36, 24},
        std::wstring_view{L"<h1>WinUI 3 Gallery</h1>"
        L"<p>Тот же каталог контролов, написанный без XAML: те же типы, "
        L"а код каждого примера показан под ним и выполняется как есть.</p>"},
    };
}

}  // namespace

wxl::Teardown wxl_launched() {
    auto entries = std::make_shared<std::vector<Entry>>();

    auto const add = [&](std::u16string_view name, FluentSymbol glyph, FrameworkElement (*page)()) {
        auto item = NavigationViewItem {
            content = name,
            icon = SymbolIcon {symbol = glyph},
        };
        entries->push_back({item, page});
        return item;
    };

    auto host = Border {};

    auto navigation = NavigationView {
        isSettingsVisible = false,
        paneDisplayMode = NavigationViewPaneDisplayMode::Left,
        isBackButtonVisible = NavigationViewBackButtonVisible::Collapsed,
        content = host,
        onSelectionChanged = [host, entries](Object const& sender, NavigationViewSelectionChangedEventArgs&) {
            auto const nav = sender.try_as<NavigationView>();
            auto const selected = nav.selectedItem();
            for (auto const& entry : *entries) {
                if (entry.item.try_as<FrameworkElement>().get_abi() == selected.try_as<FrameworkElement>().get_abi()) {
                    host.child(ScrollViewer {
                        content = Border {Padding {36, 8}, entry.page()},
                    });
                    return;
                }
            }
        },
    };

    auto home = add(u"Home", FluentSymbol::Home, &homePage);
    auto button = add(u"Button", FluentSymbol::Touch, &gallery::buttonPage);
    navigation.menuItems().append(home);
    navigation.menuItems().append(NavigationViewItemHeader {content = u"Controls"});
    navigation.menuItems().append(button);
    navigation.selectedItem(home);

    auto window = Window {
        title = u"WinUI 3 Gallery",
        minSize = {900, 560},
        systemBackdrop = MicaBackdrop {},
        content = navigation,
    };

    window.appWindow().resize({1280, 800});
    window.activate();

    return {};
}
