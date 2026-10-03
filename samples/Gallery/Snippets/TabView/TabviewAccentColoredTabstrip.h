auto const menu = gallery::tabViewContextMenu();
// The content of a tab is often a frame that contains a page, though it could be any element.
auto const newTab = [menu](int index) {
    Frame frame;
    gallery::showSample(frame, index % 3 + 1);
    return TabViewItem {header = u"Document " + gallery::numberText(index), iconSource = SymbolIconSource {symbol = Symbol::Document},
                        contextFlyout = menu, content = frame};
};
auto const addTab = [newTab](TabView const& sender) { sender.tabItems().append(newTab(static_cast<int>(sender.tabItems().size()))); };
auto const closeTab = [](TabView const& sender, TabViewTabCloseRequestedEventArgs& args) { sender.tabItems().remove(args.tab()); };
auto const threeTabs = [newTab](TabView const& self) {
    for (int i = 0; i < 3; ++i) {
        self.tabItems().append(newTab(i));
    }
};
// The strip takes its background from TabViewBackground, which this TabView says differently for each theme.
auto example = TabView {
    minHeight = 475,
    Margin {-12},
    selectedIndex = 0,
    dsl::resources = ResourceDictionary {
        themeEntry = ThemeResources {u"Light", {Resource {u"TabViewBackground", SolidColorBrush {color = resourceColor(u"SystemAccentColorLight2")}}}},
        themeEntry = ThemeResources {u"Dark", {Resource {u"TabViewBackground", SolidColorBrush {color = resourceColor(u"SystemAccentColorDark2")}}}},
    },
    onAddTabButtonClick = addTab,
    onTabCloseRequested = closeTab,
    onLoaded = threeTabs,
};