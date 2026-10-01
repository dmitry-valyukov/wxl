// The switch writes the field, the text shows it.
struct Model {
    core::observable<bool> selectable;
};
auto const model = gallery::hold<Model>();

auto text = TextBlock {
    u"I am a selectable TextBlock with custom SelectionHighlightColor.",
    isTextSelectionEnabled = BindOutput {model->selectable},
    selectionHighlightColor = SolidColorBrush {color = rgb(255, 140, 0)},
};

auto option = ToggleSwitch {
    header = u"IsTextSelectionEnabled",
    isOn = Bind {model->selectable},
};