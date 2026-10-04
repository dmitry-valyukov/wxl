// A conditional attribute is a value computed from the flags; both flags may be true at once, the program decides
// which wins (XAML throws).
auto example = Button {
    content = u"Background depends on the active flag",
    foreground = colors.white,
    background = BindOutput {flags->legacyMode, [](bool legacy) { return SolidColorBrush {color = legacy ? rgb(0xC4, 0x2B, 0x1C) : rgb(0x10, 0x7C, 0x10)}; }},
};
