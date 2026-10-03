struct Model {
    Frame frame;
    TextBox headerText {automationName = u"Header property", text = u"Header"};
    TextBox paneText {automationName = u"PaneTitle property", text = u"Pane Title"};
    NavigationViewItem second {content = u"Menu Item2", selectsOnInvoked = true, tag = u"SamplePage2", icon = SymbolIcon {symbol = Symbol::Save}};
    HyperlinkButton paneLink {Margin {12, 0}, content = u"More info", visibility = Visibility::Collapsed};
    StackPanel footer {orientation.vertical,
                       visibility = Visibility::Collapsed,
                       NavigationViewItem {automationName = u"download", icon = SymbolIcon {symbol = Symbol::Download}},
                       NavigationViewItem {automationName = u"favorite", icon = SymbolIcon {symbol = Symbol::Favorite}}};
    NavigationView view;
};
auto const model = gallery::hold<Model>();

auto const searchBox = [] { return AutoSuggestBox {automationName = u"Search", queryIcon = SymbolIcon {symbol = Symbol::Find}}; };

model->view = NavigationView {
    height = 540,
    Margin {0, 12, 0, 0},
    expandedModeThresholdWidth = 500,
    header = u"Header",
    isTabStop = false,
    paneDisplayMode = NavigationViewPaneDisplayMode::Left,
    paneTitle = u"Pane Title",
    menuItems[NavigationViewItem {content = u"Menu Item1", tag = u"SamplePage1", icon = SymbolIcon {symbol = Symbol::Play}},
              NavigationViewItemHeader {content = u"Actions"}, model->second,
              NavigationViewItem {content = u"Menu Item3", tag = u"SamplePage3", icon = SymbolIcon {symbol = Symbol::Refresh}}],
    paneCustomContent = model->paneLink,
    autoSuggestBox = searchBox(),
    paneFooter = model->footer,
    content = model->frame,
    onSelectionChanged = [model](NavigationView const& sender, NavigationViewSelectionChangedEventArgs& args) {
        if (args.isSettingsSelected()) {
            gallery::showSettings(model->frame);
            return;
        }
        auto const item = args.selectedItem().try_as<NavigationViewItem>();
        if (item) {
            auto const number = gallery::sampleNumber(item.tag());
            sender.header(gallery::sampleHeader(number));
            model->headerText.text(gallery::sampleHeader(number));
            gallery::showSample(model->frame, number);
        }
    },
};
model->headerText.add_onTextChanged([model](auto&&...) { model->view.header(model->headerText.text()); });
model->paneText.add_onTextChanged([model](auto&&...) { model->view.paneTitle(model->paneText.text()); });

auto const checked = [](CheckBox const& self) { return self.isChecked().value_or(false); };
auto options = StackPanel {
    CheckBox {content = u"Settings item visible", isChecked = true, onClick = [model, checked](CheckBox const& self) { model->view.isSettingsVisible(checked(self)); }},
    CheckBox {content = u"Back button visible", isChecked = true, onClick = [model, checked](CheckBox const& self) {
                  model->view.isBackButtonVisible(checked(self) ? NavigationViewBackButtonVisible::Visible : NavigationViewBackButtonVisible::Collapsed);
              }},
    CheckBox {content = u"Back button enabled", isChecked = false, onClick = [model, checked](CheckBox const& self) { model->view.isBackEnabled(checked(self)); }},
    CheckBox {content = u"AutoSuggestBox visible", isChecked = true, onClick = [model, checked, searchBox](CheckBox const& self) {
                  model->view.autoSuggestBox(checked(self) ? core::nullable<AutoSuggestBox> {searchBox()} : core::nullable<AutoSuggestBox> {});
              }},

    TextBlock {Margin {0, 12, 0, 0}, u"Header:"},
    model->headerText,
    CheckBox {content = u"Always show header", isChecked = true, onClick = [model, checked](CheckBox const& self) { model->view.alwaysShowHeader(checked(self)); }},
    TextBlock {Margin {0, 12, 0, 0}, u"PaneTitle:"},
    model->paneText,
    CheckBox {content = u"PaneCustomContent visible", isChecked = false, onClick = [model, checked](CheckBox const& self) {
                  model->paneLink.visibility(checked(self) ? Visibility::Visible : Visibility::Collapsed);
              }},
    CheckBox {content = u"PaneFooter visible", isChecked = false, onClick = [model, checked](CheckBox const& self) {
                  model->footer.visibility(checked(self) ? Visibility::Visible : Visibility::Collapsed);
              }},

    TextBlock {Margin {0, 12, 0, 0}, u"PanePosition:"},
    RadioButton {content = u"Left", isChecked = true, onChecked = [model](auto&&...) {
                     model->view.paneDisplayMode(NavigationViewPaneDisplayMode::Left);
                     model->view.isPaneOpen(true);
                     model->footer.orientation(Orientation::Vertical);
                 }},
    RadioButton {Margin {0, 0, 0, 12}, content = u"Top", onChecked = [model](auto&&...) {
                     model->view.paneDisplayMode(NavigationViewPaneDisplayMode::Top);
                     model->view.isPaneOpen(false);
                     model->footer.orientation(Orientation::Horizontal);
                 }},

    CheckBox {content = u"Keyboard SelectionFollowsFocus", isChecked = false, onClick = [model, checked](CheckBox const& self) {
                  model->view.selectionFollowsFocus(checked(self) ? NavigationViewSelectionFollowsFocus::Enabled : NavigationViewSelectionFollowsFocus::Disabled);
              }},
    CheckBox {content = u"Selection of Menu Item2 suppressed", isChecked = false, onClick = [model, checked](CheckBox const& self) {
                  model->second.selectsOnInvoked(!checked(self));
              }},
};

auto example = model->view;