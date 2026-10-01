const Preset dot {width = 32, height = 32, radiusX = 4.0, radiusY = 4.0};

const Preset cell {
    Padding {0},
    minWidth = 0,
    minHeight = 0,
    Margin {6},
    CornerRadius {4},
};

// One coroutine per swatch: it waits for its click, closes the flyout and
// ends. It takes the flyout as a parameter, so it lives in its own frame and
// not in a closure this function would take with it.
auto const hideOnClick = [](Flyout flyout, Button swatch) -> async::detached_task {
    auto clicks = onClick(swatch);
    if (co_await clicks.next()) {
        flyout.hide();
    }
};

auto chooser = SplitButton {
    minWidth = 0,
    minHeight = 0,
    Padding {5},
    vAlign.top,
    content = u"Choose color",
    flyout = Flyout {
        placement = FlyoutPlacementMode::Bottom,
        onOpened = [hideOnClick](Object const& sender) {
            auto const flyout = sender.try_as<Flyout>();
            auto const swatches = flyout.content().try_as<VariableSizedWrapGrid>().children();
            for (uint32_t i = 0; i < swatches.size(); ++i) {
                hideOnClick(flyout, swatches[i].try_as<Button>());
            }
        },
        content = VariableSizedWrapGrid {
            maximumRowsOrColumns = 3,
            orientation.horizontal,
            Button {cell, content = Rectangle {dot, fill = rgb(255, 0, 0)}},
            Button {cell, content = Rectangle {dot, fill = rgb(255, 165, 0)}},
            Button {cell, content = Rectangle {dot, fill = rgb(255, 255, 0)}},
            Button {cell, content = Rectangle {dot, fill = rgb(0, 128, 0)}},
            Button {cell, content = Rectangle {dot, fill = rgb(0, 0, 255)}},
            Button {cell, content = Rectangle {dot, fill = rgb(75, 0, 130)}},
            Button {cell, content = Rectangle {dot, fill = rgb(238, 130, 238)}},
            Button {cell, content = Rectangle {dot, fill = rgb(128, 128, 128)}},
            Button {cell, content = Rectangle {dot, fill = rgb(0, 0, 0)}},
        },
    },
};