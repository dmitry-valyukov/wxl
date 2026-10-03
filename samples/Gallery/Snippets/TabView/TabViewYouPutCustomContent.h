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
auto example = StackPanel {
    TextBlock {Margin {0, 0, 0, 12}, textWrapping = TextWrapping::WrapWholeWords, u"You can put any content in the TabStripHeader and TabStripFooter areas"},
    TextBlock {Margin {0, 0, 0, 12}, textWrapping = TextWrapping::WrapWholeWords,
               u"If your TabView is used inside the app's titlebar area, use the TabStripFooter to specify a custom drag region"},
    TextBlock {Margin {0, 0, 0, 24}, textWrapping = TextWrapping::WrapWholeWords, u"See TabViewWindowingSamplePage.xaml and *.cs files to see the complete code"},

    TabView {
        // Raised when the width mode is SizeToContent or Compact; handled here to keep the page from scrolling by itself as it loads.
        onBringIntoViewRequested = [](auto const&, BringIntoViewRequestedEventArgs& args) { args.handled(true); },
        minHeight = 475,
        Margin {-12},
        selectedIndex = 0,
        tabWidthMode = TabViewWidthMode::SizeToContent,
        tabStripHeader = TextBlock {Margin {8, 6}, vAlign.center, styles.TextBlock.Base, u"TabStripHeader Content"},
        tabStripFooter = TextBlock {Margin {6}, hAlign.right, vAlign.center, styles.TextBlock.Base, u"TabStripFooter Content"},
        onAddTabButtonClick = addTab,
        onTabCloseRequested = closeTab,
        onLoaded = threeTabs,
    },
};