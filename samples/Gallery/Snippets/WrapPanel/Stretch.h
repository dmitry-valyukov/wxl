struct Model {
    core::observable<bool> stretchLast{true};
};
auto const model = gallery::hold<Model>();

auto item = [](char16_t const* text) {
    return Border {
        width = 72,
        Padding {12, 8},
        background = brushes.Control.FillColor.Default,
        borderBrush = brushes.Control.StrokeColor.Default,
        BorderThickness {1},
        CornerRadius {4},
        TextBlock {text},
    };
};

auto panelHost = Border {
    width = 360,
    hAlign.left,
    background = brushes.Card.BackgroundFillColor.Secondary,
    borderBrush = brushes.Card.StrokeColorDefault,
    BorderThickness {1},
    CornerRadius {4},
    WrapPanel {
        itemSpacing = 8.0,
        itemsStretch = BindOutput {model->stretchLast, [](bool last) {
                                       return last ? WrapPanelItemsStretch::Last : WrapPanelItemsStretch::None;
                                   }},
        Padding {12},
        item(u"One"),
        item(u"Two"),
        item(u"Three"),
        item(u"Four"),
        item(u"Five"),
        Border {
            Padding {12, 8},
            background = brushes.Accent.FillColor.Default,
            borderBrush = brushes.Control.StrokeColor.Default,
            BorderThickness {1},
            CornerRadius {4},
            TextBlock {u"Final item", foreground = brushes.Text.OnAccent.FillColor.Primary},
        },
    },
};

auto stretchToggle = ToggleSwitch {
    automationId = u"WrapPanelStretchLastItem",
    automationName = u"Stretch final item",
    header = u"ItemsStretch",
    onContent = u"Last",
    offContent = u"None",
    isOn = Bind {model->stretchLast},
};