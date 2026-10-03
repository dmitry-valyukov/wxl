struct Model {
    Frame frame;
};
auto const model = gallery::hold<Model>();

auto const view = NavigationView {
    row = 1,
    height = 460,
    isTabStop = false,
    paneDisplayMode = NavigationViewPaneDisplayMode::Left,
    menuItems[NavigationViewItem {content = u"Home", icon = SymbolIcon {symbol = Symbol::Home}, tag = u"SamplePage1", toolTip = u"Home"},
              NavigationViewItem {content = u"Account",
                                  icon = SymbolIcon {symbol = Symbol::Contact},
                                  tag = u"SamplePage2",
                                  toolTip = u"Account",
                                  menuItems[NavigationViewItem {content = u"Mail", icon = SymbolIcon {symbol = Symbol::Mail}, tag = u"SamplePage3", toolTip = u"Mail"},
                                            NavigationViewItem {content = u"Calendar", icon = SymbolIcon {symbol = Symbol::Calendar}, tag = u"SamplePage4",
                                                                toolTip = u"Calendar"}]},
              NavigationViewItem {content = u"Document options",
                                  icon = SymbolIcon {symbol = Symbol::Page2},
                                  selectsOnInvoked = false,
                                  toolTip = u"Document options",
                                  menuItems[NavigationViewItem {content = u"Create new", icon = SymbolIcon {symbol = Symbol::NewFolder}, tag = u"SamplePage5",
                                                                toolTip = u"Create new"},
                                            NavigationViewItem {content = u"Upload file", icon = SymbolIcon {symbol = Symbol::OpenLocal}, tag = u"SamplePage6",
                                                                toolTip = u"Upload file"}]}],
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

auto const mode = [view](NavigationViewPaneDisplayMode display, bool open) {
    return [view, display, open](auto&&...) {
        view.paneDisplayMode(display);
        view.isPaneOpen(open);
    };
};
auto options = StackPanel {
    TextBlock {Margin {0, 12, 0, 0}, u"PanePosition:"},
    RadioButton {content = u"Left mode", groupName = u"hierachicalGroup", isChecked = true, onChecked = mode(NavigationViewPaneDisplayMode::Left, true)},
    RadioButton {content = u"Top mode", groupName = u"hierachicalGroup", onChecked = mode(NavigationViewPaneDisplayMode::Top, false)},
    RadioButton {content = u"LeftCompact mode", groupName = u"hierachicalGroup", onChecked = mode(NavigationViewPaneDisplayMode::LeftCompact, false)},
};

auto example = Grid {
    rowDefinitions = u"auto,auto",
    RichTextBlock {
        Margin {0, 0, 0, 15},
        textWrapping = TextWrapping::Wrap,
        Paragraph {Run {u"NavigationView supports hierarchy in Left, LeftCompact, and Top display modes."}, LineBreak {}},
        Paragraph {Run {u"In the example below, the \"Account\" tab navigates to its own page while \"Document options\" only opens up its subtree of "
                        u"items. This is done by setting the SelectsOnInvoked property to false on the Document options NavigationView Item."},
                   LineBreak {}},
        Paragraph {Run {u"In both Top and Left modes, clicking the arrows on NavigationViewItems will expand or collapse the subtree. Clicking or "
                        u"tapping elsewhere on the NavigationViewItem will collapse or expand the subtree."},
                   LineBreak {}},
        Paragraph {Run {u"Switch between the three pane display modes on the right."}},
    },
    view,
};