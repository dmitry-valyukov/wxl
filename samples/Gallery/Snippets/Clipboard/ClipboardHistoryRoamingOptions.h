struct Model {
    TextBox box {width = 400, horizontalAlignment = HorizontalAlignment::Left, automationName = u"Text to copy with options", placeholderText = u"Enter text to copy with options",
                 u"Text with clipboard options"};
    ToggleSwitch history {header = u"Allow in History", automationName = u"Allow clipboard content in history", isOn = true};
    ToggleSwitch roaming {header = u"Allow Roaming", automationName = u"Allow clipboard content to roam", isOn = true};
    TextBlock status {textWrapping = TextWrapping::Wrap};
    TextBlock historyState;
    TextBlock roamingState;

    void copy() {
        auto const words = box.text();
        if (words.empty()) {
            status.text(u"Please enter text to copy.");
            return;
        }
        auto package = DataPackage {};
        package.setText(words);

        auto options = ClipboardContentOptions {isAllowedInHistory = history.isOn(), isRoamable = roaming.isOn()};
        if (Clipboard::setContentWithOptions(package, options)) {
            std::u16string report = u"Text copied to clipboard.";
            report += options.isAllowedInHistory() ? u" History: allowed." : u" History: excluded.";
            report += options.isRoamable() ? u" Roaming: allowed." : u" Roaming: excluded.";
            status.text(report);
        } else {
            status.text(u"Error copying content to clipboard.");
        }
    }
};
auto const model = gallery::hold<Model>();

// The system may not offer these two questions at all.
try {
    model->historyState.text(Clipboard::isHistoryEnabled() ? u"Clipboard history: enabled" : u"Clipboard history: disabled");
    model->roamingState.text(Clipboard::isRoamingEnabled() ? u"Clipboard roaming: enabled" : u"Clipboard roaming: disabled");
} catch (...) {
}

auto example = StackPanel {
    verticalAlignment = VerticalAlignment::Top,
    spacing = 10,
    model->box,
    StackPanel {orientation.horizontal, spacing = 16, model->history, model->roaming},
    Button {content = u"Copy with Options", onClick = [model](Button const&) { model->copy(); }},
    model->status,
    StackPanel {orientation.horizontal, spacing = 16, model->historyState, model->roamingState},
};