// Three levels of resources. Application level: what the framework and the application keep for every page --
// read by key with resourceColor. Page level: a value of the function that builds the page. Control level: a
// dictionary of the control itself (dsl::resources), where what a ResourceDictionary of XAML keeps lives.
auto const primary = resourceColor(u"SystemAccentColor");
auto const highlight = SolidColorBrush {color = rgb(0xA9, 0x4D, 0xC1)};
auto const fontColor = SolidColorBrush {color = rgb(255, 255, 255)};

auto example = StackPanel {
    Padding {8},
    background = SolidColorBrush {color = primary},
    cornerRadius = CornerRadius {4},
    TextBlock {fontSize = 24, foreground = fontColor, u"Using application-level resources"},
    StackPanel {
        Margin {8},
        Padding {8},
        background = highlight,
        cornerRadius = CornerRadius {4},
        TextBlock {fontSize = 18, foreground = fontColor, u"Using page-level resources"},
        StackPanel {
            Margin {8},
            Padding {8},
            cornerRadius = CornerRadius {4},
            dsl::resources = ResourceDictionary {
                entry = Resource {u"BackgroundColor", rgb(0xE2, 0x24, 0x1A)},
                entry = Resource {u"Description", std::u16string {u"Using control-level resources"}},
            },
            Grid {Padding {8}, background = SolidColorBrush {color = rgb(0xE2, 0x24, 0x1A)}, cornerRadius = CornerRadius {4},
                  TextBlock {foreground = fontColor, u"Using control-level resources"}},
        },
    },
};
