auto badge = InfoBadge {value = 5};

auto navigation = NavigationView {height = 300, paneDisplayMode = NavigationViewPaneDisplayMode::Left};

auto opacity = ToggleSwitch {
    header = u"InfoBadge Opacity",
    isOn = true,
    onToggled = [badge](ToggleSwitch const& self) { badge.opacity(self.isOn() ? 1.0 : 0.0); },
};

auto displayMode = ComboBox {
    header = u"Display Mode",
    ComboBoxItem {content = u"LeftExpanded"},
    ComboBoxItem {content = u"LeftCompact"},
    ComboBoxItem {content = u"Top"},
    selectedIndex = 0,
    onSelectionChanged = [navigation](ComboBox const& self) {
        switch (self.selectedIndex()) {
            case 0:
                navigation.paneDisplayMode(NavigationViewPaneDisplayMode::Left);
                navigation.isPaneOpen(true);
                break;
            case 1:
                navigation.paneDisplayMode(NavigationViewPaneDisplayMode::LeftCompact);
                navigation.isPaneOpen(false);
                break;
            case 2:
                navigation.paneDisplayMode(NavigationViewPaneDisplayMode::Top);
                navigation.isPaneOpen(true);
                break;
        }
    },
};

navigation.menuItems().append(NavigationViewItem {content = u"Home", icon = SymbolIcon {symbol = FluentSymbol::Home}});
navigation.menuItems().append(NavigationViewItem {content = u"Account", icon = SymbolIcon {symbol = FluentSymbol::Contact}});
navigation.menuItems().append(NavigationViewItem {
    content = u"Inbox",
    icon = SymbolIcon {symbol = FluentSymbol::Mail},
    infoBadge = badge,
});