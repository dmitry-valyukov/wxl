struct Model {
    Frame frame;
};
auto const model = gallery::hold<Model>();

auto const view = NavigationView {
    row = 1,
    height = 460,
    header = u"This is Header Text",
    isSettingsVisible = false,
    isTabStop = false,
    paneDisplayMode = NavigationViewPaneDisplayMode::Left,
    menuItems[NavigationViewItem {content = u"Browse", icon = SymbolIcon {symbol = Symbol::Library}, tag = u"SamplePage1"},
              NavigationViewItem {content = u"Track an Order", icon = SymbolIcon {symbol = Symbol::Map}, tag = u"SamplePage2"},
              NavigationViewItem {content = u"Order History", icon = SymbolIcon {symbol = Symbol::Tag}, tag = u"SamplePage3"}],
    footerMenuItems[NavigationViewItem {content = u"Account", icon = SymbolIcon {symbol = Symbol::Contact}, tag = u"SamplePage4"},
                    NavigationViewItem {content = u"Your Cart", icon = SymbolIcon {symbol = Symbol::Shop}, tag = u"SamplePage5"},
                    NavigationViewItem {content = u"Help", icon = SymbolIcon {symbol = Symbol::Help}, tag = u"SamplePage5"}],
    content = model->frame,
    onSelectionChanged = [model](NavigationView const&, NavigationViewSelectionChangedEventArgs& args) {
        gallery::showSample(model->frame, gallery::sampleNumber(args.selectedItem().try_as<NavigationViewItem>().tag()),
                            args.recommendedNavigationTransitionInfo());
    },
};
view.selectedItem(view.menuItems()[0]);

auto options = StackPanel {RadioButtons {
    header = u"Pane position:",
    selectedIndex = 0,
    RadioButton {content = u"Left mode", onChecked = [view](auto&&...) {
                     view.paneDisplayMode(NavigationViewPaneDisplayMode::Left);
                     view.isPaneOpen(true);
                 }},
    RadioButton {content = u"Top mode", onChecked = [view](auto&&...) {
                     view.paneDisplayMode(NavigationViewPaneDisplayMode::Top);
                     view.isPaneOpen(false);
                 }},
}};

auto example = Grid {
    rowDefinitions = u"*,auto",
    TextBlock {Margin {0, 0, 0, 12}, textWrapping = TextWrapping::WrapWholeWords,
               u"You can add clickable menu items to the footer of your NavigationView that participate in the same selection model as items in "
               u"the main menu. In Top PaneDisplayMode, these items will appear aligned to the right of the NavigationView. In Left PaneDisplayMode, "
               u"these items will appear aligned to the bottom of the NavigationView. "},
    view,
};