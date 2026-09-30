// The check box and the bar's own close button say the same thing.
struct Model {
    core::observable<bool> open{true};
};
auto const model = gallery::hold<Model>();

auto bar = InfoBar {
    title = u"Title",
    isOpen = BindOutput {model->open},
    message = u"Essential app message for your users to be informed of, acknowledge, or take action on.",
    severity = InfoBarSeverity::Informational,
    onClosed = [model](InfoBar const&, InfoBarClosedEventArgs&) { model->open.set(false); },
};

auto isOpen = CheckBox {content = u"Is Open", isChecked = Bind {model->open}};

auto severity = ComboBox {
    hAlign.stretch,
    header = u"Severity",
    ComboBoxItem {content = u"Informational"},
    ComboBoxItem {content = u"Success"},
    ComboBoxItem {content = u"Warning"},
    ComboBoxItem {content = u"Error"},
    selectedIndex = 0,
    onSelectionChanged = [bar](ComboBox const& self) {
        static constexpr InfoBarSeverity kinds[] = {InfoBarSeverity::Informational, InfoBarSeverity::Success,
                                                    InfoBarSeverity::Warning, InfoBarSeverity::Error};
        int const index = self.selectedIndex();
        if (index >= 0) {
            bar.severity(kinds[index]);
        }
    },
};