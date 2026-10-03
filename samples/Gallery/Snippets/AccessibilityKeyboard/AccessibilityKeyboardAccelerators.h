auto const swatch = Rectangle {columnSpan = 3, height = 30, fill = colors.red, radiusX = 4.0, radiusY = 4.0};
auto const paint = [swatch](Color shade) {
    return [swatch, shade](auto&&...) { swatch.fill(SolidColorBrush {color = shade}); };
};

// Chartreuse is an uncommon color, so we explain it in the tooltip. This replaces the auto-generated tooltip, so we
// manually mention the hotkey in the custom tooltip.
auto example = Grid {
    columnSpacing = 8.0,
    rowSpacing = 8.0,
    rowDefinitions = u"auto,auto,auto",
    columnDefinitions = u"auto,auto,auto,*",
    swatch,
    Button {
        row = 1,
        automationAcceleratorKey = u"Ctrl+R",
        content = u"Red",
        onClick = paint(colors.red),
        keyboardAccelerators[KeyboardAccelerator {key = VirtualKey::R, modifiers = VirtualKeyModifiers::Control}],
    },
    Button {
        row = 1,
        column = 1,
        automationAcceleratorKey = u"Ctrl+B",
        content = u"Blue",
        onClick = paint(colors.blue),
        keyboardAccelerators[KeyboardAccelerator {key = VirtualKey::B, modifiers = VirtualKeyModifiers::Control}],
    },
    Button {
        row = 1,
        column = 2,
        automationAcceleratorKey = u"Ctrl+G",
        content = u"Chartreuse",
        toolTip = u"A greenish-yellow (Ctrl+G)",
        onClick = paint(rgb(127, 255, 0)),
        keyboardAccelerators[KeyboardAccelerator {key = VirtualKey::G, modifiers = VirtualKeyModifiers::Control}],
    },
    TextBlock {row = 2, columnSpan = 4, styles.TextBlock.Caption, u"Ctrl+R, Ctrl+B, and Ctrl+G trigger Red, Blue, and Chartreuse respectively"},
};
