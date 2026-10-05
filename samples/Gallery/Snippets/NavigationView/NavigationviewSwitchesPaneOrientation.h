struct Model {
    Frame frame;
};
auto const model = gallery::hold<Model>();

auto const view = NavigationView {
    row = 1,
    height = 460,
    isTabStop = false,
    paneDisplayMode = NavigationViewPaneDisplayMode::Auto,
    menuItems[NavigationViewItem {content = u"Menu Item1", tag = u"SamplePage1"}, NavigationViewItem {content = u"Menu Item2", tag = u"SamplePage2"},
              NavigationViewItem {content = u"Menu Item3", tag = u"SamplePage3"}, NavigationViewItem {content = u"Menu Item4", tag = u"SamplePage4"}],
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
view.updateLayout();

// What the AdaptiveTrigger of the original says: from the compact-mode width of the window up, the pane is on top; below it, the
// pane is as it is by default -- a minimal one on the left.
auto const adapt = [view](auto&&...) {
    if (auto const root = view.xamlRoot()) {
        view.paneDisplayMode(root.size().width >= view.compactModeThresholdWidth() ? NavigationViewPaneDisplayMode::Top
                                                                                  : NavigationViewPaneDisplayMode::Auto);
    }
};
view.add_onLoaded([view, adapt](auto&&...) {
    view.xamlRoot().add_onChanged(adapt);
    adapt();
});

auto example = Grid {
    rowDefinitions = u"auto,auto",
    TextBlock {Margin {0, 0, 0, 12}, textWrapping = TextWrapping::WrapWholeWords,
               u"If you have equally important navigation categories and limited app content space, consider using a top navigation pane on larger "
               u"window widths and a minimal left navigation pane on smaller window widths."},
    view,
};