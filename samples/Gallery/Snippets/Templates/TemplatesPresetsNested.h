// A preset keeps its arguments and wears them on whatever has the properties it names. A preset inside another one's
// braces is an argument like any other, so a look is built up from smaller looks.
auto const text = Preset {fontSize = 16, vAlign.center};
auto const strong = Preset {text, FontWeight {600}};
auto const warning = Preset {strong, foreground = brushes.SystemFillColor.Critical};
auto const note = Preset {text, foreground = brushes.Text.FillColor.Secondary, textWrapping = TextWrapping::Wrap};

auto const lastWord = Button {content = u"Worn after the building"};
Apply {lastWord, warning};  // the same preset, applied to an object that already exists

auto example = StackPanel {
    spacing = 8.0,
    TextBlock {text, u"A preset: size and alignment."},
    TextBlock {strong, u"A preset in a preset: and the weight."},
    TextBlock {warning, u"A preset in a preset in a preset: and the color."},
    // Presets fit what has the properties, not a class: the same one dresses a Button.
    Button {strong, content = u"The strong preset on a Button"},
    lastWord,
    // A preset in a real description, among its own arguments, in any order.
    TextBlock {note, Margin {0, 8, 0, 0}, u"Where a preset stands in the braces is only the order in which its arguments are applied: "
                                          u"what comes after it overrides it."},
};
