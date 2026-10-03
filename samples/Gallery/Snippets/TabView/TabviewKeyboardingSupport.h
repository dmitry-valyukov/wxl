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
auto const tabs = TabView {
    minHeight = 475,
    Margin {-12},
    selectedIndex = 0,
    onAddTabButtonClick = addTab,
    onTabCloseRequested = closeTab,
    onLoaded = threeTabs,
};

tabs.keyboardAccelerators().append(KeyboardAccelerator {
    key = VirtualKey::T,
    modifiers = VirtualKeyModifiers::Control,
    onInvoked = [newTab](auto const&, KeyboardAcceleratorInvokedEventArgs& args) {
        if (args.element().is<TabView>()) {
            auto const view = args.element().try_as<TabView>();
            view.tabItems().append(newTab(static_cast<int>(view.tabItems().size())));
        }
        args.handled(true);
    },
});
tabs.keyboardAccelerators().append(KeyboardAccelerator {
    key = VirtualKey::W,
    modifiers = VirtualKeyModifiers::Control,
    onInvoked = [](auto const&, KeyboardAcceleratorInvokedEventArgs& args) {
        if (args.element().is<TabView>()) {
            auto const view = args.element().try_as<TabView>();
            // Only close the selected tab if it is closeable.
            auto const selected = view.selectedItem();
            if (selected && selected.is<TabViewItem>() && selected.try_as<TabViewItem>().isClosable()) {
                view.tabItems().remove(selected);
            }
        }
        args.handled(true);
    },
});
for (int number = 1; number <= 9; ++number) {
    tabs.keyboardAccelerators().append(KeyboardAccelerator {
        key = static_cast<VirtualKey>(static_cast<int>(VirtualKey::Number1) + number - 1),
        modifiers = VirtualKeyModifiers::Control,
        onInvoked = [number](auto const&, KeyboardAcceleratorInvokedEventArgs& args) {
            if (args.element().is<TabView>()) {
                auto const view = args.element().try_as<TabView>();
                // Ctrl+9 selects the last tab, whatever their number.
                auto const wanted = number == 9 ? static_cast<int>(view.tabItems().size()) - 1 : number - 1;
                if (wanted < static_cast<int>(view.tabItems().size())) {
                    view.selectedIndex(wanted);
                }
            }
            args.handled(true);
        },
    });
}

auto example = StackPanel {
    TextBlock {textWrapping = TextWrapping::WrapWholeWords, u"- Ctrl+T opens a new tab"},
    TextBlock {textWrapping = TextWrapping::WrapWholeWords, u"- Ctrl+W closes the selected tab"},
    TextBlock {textWrapping = TextWrapping::WrapWholeWords, u"- Ctrl+1 to Ctrl+8 selects that number tab"},
    TextBlock {Margin {0, 0, 0, 24}, textWrapping = TextWrapping::WrapWholeWords, u"- Ctrl+9 selects the last tab (regardless of the number of tabs)"},
    tabs,
};