struct Model {
    Frame frame;
    core::observable<double> room;
    core::observable<bool> top;
};
auto const model = gallery::hold<Model>();

auto const view = NavigationView {
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

// What the AdaptiveTrigger of the original says of the window, said of the room the view has: from the compact-mode width up
// the pane is on top; below it the pane is as it is by default -- a minimal one on the left. The layout of the panel around
// the view reports the room into the model, and the mode follows it.
model->top.follow(model->room, [view](double room) { return room >= view.compactModeThresholdWidth(); });
Apply {view, paneDisplayMode = BindOutput {model->top, [](bool top) {
                 return top ? NavigationViewPaneDisplayMode::Top : NavigationViewPaneDisplayMode::Auto;
             }}};

auto example = Grid {
    rowDefinitions = u"auto,auto",
    TextBlock {Margin {0, 0, 0, 12}, textWrapping = TextWrapping::WrapWholeWords,
               u"If you have equally important navigation categories and limited app content space, consider using a top navigation pane on larger "
               u"window widths and a minimal left navigation pane on smaller window widths."},
    LayoutPanel {row = 1, layout = AvailableSizeLayout {availableWidth = BindInput {model->room}}, view},
};