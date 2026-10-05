auto const menu = gallery::tabViewContextMenu();
auto const documentTab = [menu](char16_t const* title, Symbol symbol, int page) {
    return TabViewItem {header = title, contextFlyout = menu, iconSource = SymbolIconSource {symbol = symbol}, content = gallery::samplePage(page)};
};

auto const tabs = TabView {
    minHeight = 475,
    Margin {-12},
    isAddTabButtonVisible = false,
    selectedIndex = 0,
    tabItems[documentTab(u"Home", Symbol::Home, 1), documentTab(u"Tab 2 Has Longer Text", Symbol::MusicInfo, 2), documentTab(u"Third Tab", Symbol::Placeholder, 3)],
};

auto const overlay = [tabs](int index) {
    static constexpr TabViewCloseButtonOverlayMode modes[] = {TabViewCloseButtonOverlayMode::Auto, TabViewCloseButtonOverlayMode::Always,
                                                             TabViewCloseButtonOverlayMode::OnPointerOver};
    tabs.closeButtonOverlayMode(modes[std::max(index, 0)]);
};
overlay(1);

auto example = tabs;
auto options = ComboBox {
    width = 150,
    header = u"TabViewItem CloseButtonOverlayMode",
    ComboBoxItem {content = u"Auto"},
    ComboBoxItem {content = u"Always"},
    ComboBoxItem {content = u"OnHover"},
    selectedIndex = 1,
    onSelectionChanged = [overlay](ComboBox const& self) { overlay(self.selectedIndex()); },
};