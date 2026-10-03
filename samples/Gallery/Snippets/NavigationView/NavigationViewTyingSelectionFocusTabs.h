struct Model {
    Frame frame;
};
auto const model = gallery::hold<Model>();

auto const view = NavigationView {
    row = 1,
    height = 460,
    isBackButtonVisible = NavigationViewBackButtonVisible::Collapsed,
    isTabStop = false,
    paneDisplayMode = NavigationViewPaneDisplayMode::Top,
    selectionFollowsFocus = NavigationViewSelectionFollowsFocus::Enabled,
    menuItems[NavigationViewItem {content = u"Item1", tag = u"SamplePage1"}, NavigationViewItem {content = u"Item2", tag = u"SamplePage2"},
              NavigationViewItem {content = u"Item3", tag = u"SamplePage3"}, NavigationViewItem {content = u"Item4", tag = u"SamplePage4"}],
    content = model->frame,
    onSelectionChanged = [model](NavigationView const&, NavigationViewSelectionChangedEventArgs& args) {
        if (args.isSettingsSelected()) {
            gallery::showSettings(model->frame);
            return;
        }
        // The Frame is told what the navigation view recommends and, with selection following focus, keeps no stack of the tabs.
        gallery::showSample(model->frame, gallery::sampleNumber(args.selectedItem().try_as<NavigationViewItem>().tag()),
                            args.recommendedNavigationTransitionInfo());
    },
};
view.selectedItem(view.menuItems()[0]);

auto example = Grid {
    rowDefinitions = u"auto,auto",
    TextBlock {Margin {0, 0, 0, 12}, textWrapping = TextWrapping::WrapWholeWords,
               u"For the tabs pattern, ensure that you unify selection and focus by setting the SelectionFollowsFocus property to Enabled. If using "
               u"a Frame to swap out content, then navigating between items shouldn't be recorded into the Frame's navigation stack. Please see "
               u"the C# in the sample below to understand how to do this."},
    view,
};