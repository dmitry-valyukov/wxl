auto example = StackPanel {
    spacing = 4.0,
    Button {content = u"First"},
    // Non-interactive controls should not be in the tab order
    TextBlock {u"(not present)"},
    Button {content = u"Second"},
    // Disabled controls should not be in the tab order
    Button {content = u"(not present)", isEnabled = false},
    Button {content = u"Third"},
};
