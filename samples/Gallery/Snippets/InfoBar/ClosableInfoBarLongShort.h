struct Model {
    core::observable<bool> open{true};
};
auto const model = gallery::hold<Model>();

auto bar = InfoBar {
    title = u"Title",
    isOpen = BindOutput {model->open},
    onClosed = [model](InfoBar const&, InfoBarClosedEventArgs&) { model->open.set(false); },
};

auto isOpen = CheckBox {content = u"Is Open", isChecked = Bind {model->open}};

auto length = ComboBox {
    hAlign.stretch,
    header = u"Message Length",
    ComboBoxItem {content = u"Short"},
    ComboBoxItem {content = u"Long"},
    onSelectionChanged = [bar](ComboBox const& self) {
        if (self.selectedIndex() == 0) {
            bar.message(u"A short essential app message.");
        } else if (self.selectedIndex() == 1) {
            bar.message(
                u"A long essential app message for your users to be informed of, acknowledge, or take action on. "
                u"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin dapibus dolor vitae justo rutrum, "
                u"ut lobortis nibh mattis. Aenean id elit commodo, semper felis nec.");
        }
    },
    selectedIndex = 1,
};

auto actionButton = ComboBox {
    hAlign.stretch,
    header = u"Action Button",
    ComboBoxItem {content = u"None"},
    ComboBoxItem {content = u"Button"},
    ComboBoxItem {content = u"Hyperlink"},
    selectedIndex = 0,
    onSelectionChanged = [bar](ComboBox const& self) {
        switch (self.selectedIndex()) {
            case 0:
                // A wrapper is never empty, so "no button" is a button that is not shown.
                bar.actionButton(Button {visibility = Visibility::Collapsed});
                break;
            case 1:
                bar.actionButton(Button {content = u"Action"});
                break;
            case 2:
                bar.actionButton(HyperlinkButton {content = u"Informational link", navigateUri = u"http://www.microsoft.com/"});
                break;
        }
    },
};