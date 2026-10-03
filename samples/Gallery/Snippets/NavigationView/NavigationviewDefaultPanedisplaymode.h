struct Model {
    Frame frame;
};
auto const model = gallery::hold<Model>();

auto const view = NavigationView {
    row = 1,
    height = 460,
    header = u"This is Header Text",
    isTabStop = false,
    paneDisplayMode = NavigationViewPaneDisplayMode::Auto,
    menuItems[NavigationViewItem {content = u"Menu Item1", icon = SymbolIcon {symbol = Symbol::Play}, tag = u"SamplePage1"},
              NavigationViewItem {content = u"Menu Item2", icon = SymbolIcon {symbol = Symbol::Save}, tag = u"SamplePage2"},
              NavigationViewItem {content = u"Menu Item3", icon = SymbolIcon {symbol = Symbol::Refresh}, tag = u"SamplePage3"},
              NavigationViewItem {content = u"Menu Item4", icon = SymbolIcon {symbol = Symbol::Download}, tag = u"SamplePage4"}],
    content = model->frame,
    onSelectionChanged = [model](NavigationView const& sender, NavigationViewSelectionChangedEventArgs& args) {
        if (args.isSettingsSelected()) {
            gallery::showSettings(model->frame);
            return;
        }
        auto const number = gallery::sampleNumber(args.selectedItem().try_as<NavigationViewItem>().tag());
        sender.header(gallery::sampleHeader(number));
        gallery::showSample(model->frame, number);
    },
};
view.selectedItem(view.menuItems()[0]);

auto example = Grid {
    rowDefinitions = u"auto,auto",
    TextBlock {Margin {0, 0, 0, 12}, textWrapping = TextWrapping::WrapWholeWords,
               u"If you have five or more equally important navigation categories that should prominently appear on larger window widths, consider "
               u"using a left navigation pane."},
    view,
};