auto const current = std::make_shared<Color>(rgb(0, 128, 0));

auto richBox = RichEditBox {
    width = 240,
    minHeight = 96,
    placeholderText = u"Type something here",
    onTextChanged = [current](RichEditBox const& box) {
        box.document().selection().characterFormat().foregroundColor(*current);
    },
};

auto swatch = Border {
    width = 32,
    height = 32,
    CornerRadius {4, 0, 0, 4},
    background = SolidColorBrush {color = *current},
};

auto colorButton = SplitButton {
    minWidth = 0,
    minHeight = 0,
    Padding {0},
    vAlign.top,
    content = swatch,
    onClick = [richBox, current] {
        richBox.document().selection().characterFormat().foregroundColor(*current);
    },
};

const Preset dot {width = 32, height = 32, radiusX = 4.0, radiusY = 4.0};

colorButton.flyout(Flyout {
    placement = FlyoutPlacementMode::Bottom,
    content = GridView {
        isItemClickEnabled = true,
        width = 3 * 52,
        onItemClick = [richBox, swatch, current, colorButton](Object const&, ItemClickEventArgs& args) {
            auto const picked = args.clickedItem().try_as<Rectangle>().fill().try_as<SolidColorBrush>().color();
            *current = picked;
            richBox.document().selection().characterFormat().foregroundColor(picked);
            swatch.background(SolidColorBrush {color = picked});
            colorButton.flyout().hide();
        },
        Rectangle {dot, fill = rgb(255, 0, 0)},
        Rectangle {dot, fill = rgb(255, 165, 0)},
        Rectangle {dot, fill = rgb(255, 255, 0)},
        Rectangle {dot, fill = rgb(0, 128, 0)},
        Rectangle {dot, fill = rgb(0, 0, 255)},
        Rectangle {dot, fill = rgb(75, 0, 130)},
        Rectangle {dot, fill = rgb(238, 130, 238)},
        Rectangle {dot, fill = rgb(128, 128, 128)},
    },
});

richBox.document().selection().characterFormat().foregroundColor(*current);
richBox.document().selection().setText(
    TextSetOptions::None,
    u"Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
    u"sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
    u"Tempor commodo ullamcorper a lacus.");