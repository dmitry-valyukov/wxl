struct Model {
    core::observable<bool> open{true};
    core::observable<int> length {1};
};
auto const model = gallery::hold<Model>();

auto bar = InfoBar {
    title = u"Title",
    isOpen = BindOutput {model->open},
    message = BindOutput {model->length, [](int index) {
        return index == 0 ? u"A short essential app message."
                          : u"A long essential app message for your users to be informed of, acknowledge, or take action on. "
                            u"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin dapibus dolor vitae justo rutrum, "
                            u"ut lobortis nibh mattis. Aenean id elit commodo, semper felis nec.";
    }},
    onClosed = [model](InfoBar const&, InfoBarClosedEventArgs&) { model->open.set(false); },
};

auto isOpen = CheckBox {content = u"Is Open", isChecked = Bind {model->open}};

auto length = ComboBox {
    hAlign.stretch,
    header = u"Message Length",
    ComboBoxItem {content = u"Short"},
    ComboBoxItem {content = u"Long"},
    selectedIndex = Bind {model->length},
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