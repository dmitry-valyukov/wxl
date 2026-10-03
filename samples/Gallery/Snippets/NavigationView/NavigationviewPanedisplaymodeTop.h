struct Model {
    Frame frame;
};
auto const model = gallery::hold<Model>();

auto const view = NavigationView {
    row = 1,
    height = 460,
    header = u"This is Header Text",
    isTabStop = false,
    paneDisplayMode = NavigationViewPaneDisplayMode::Top,
    menuItems[NavigationViewItem {content = u"Menu Item1", tag = u"SamplePage1"}, NavigationViewItem {content = u"Menu Item2", tag = u"SamplePage2"},
              NavigationViewItem {content = u"Menu Item3", tag = u"SamplePage3"}, NavigationViewItem {content = u"Menu Item4", tag = u"SamplePage3"}],
    content = model->frame,
    onSelectionChanged = [model](NavigationView const&, NavigationViewSelectionChangedEventArgs& args) {
        if (args.isSettingsSelected()) {
            gallery::showSettings(model->frame);
            return;
        }
        gallery::showSample(model->frame, gallery::sampleNumber(args.selectedItem().try_as<NavigationViewItem>().tag()));
    },
};
view.selectedItem(view.menuItems()[0]);

auto example = Grid {
    rowDefinitions = u"auto,*",
    TextBlock {Margin {0, 0, 0, 12}, textWrapping = TextWrapping::WrapWholeWords,
               u"If you have equally important navigation categories that should be de-emphasized relative to the content of your app, consider using "
               u"a top navigation pane."},
    view,
};