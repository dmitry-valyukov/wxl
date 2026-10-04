// A style of XAML is a preset here: the setters, kept in a variable and worn by as many controls as you like. BasedOn is a
// preset written inside another one; a setter written after the preset overrides it.
auto const customButton = Preset {background = brushes.Accent.Acrylic.BackgroundFillColorDefault, minWidth = 200};

auto example = StackPanel {
    spacing = 8.0,
    Button {content = u"Default style"},
    Button {customButton, content = u"Custom style"},
    Button {customButton, background = brushes.SystemFillColor.CriticalBackground, content = u"Custom style with overridden background"},
};
