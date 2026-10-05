// A change of Opacity runs over the duration of the transition instead of happening at once.
auto const rectangle = Rectangle {width = 50, height = 50, Margin {45, 5, 5, 5}, vAlign.center, fill = resourceBrush(u"SystemAccentColor"),
                                  opacity = 0.5, opacityTransition = ScalarTransition {}};

// The number in the box, 0 when it has none.
auto const numberOf = [](NumberBox const& box) {
    if (std::isnan(box.value())) {
        box.value(0.0);
    }
    return static_cast<float>(box.value());
};

auto const box = NumberBox {Margin {5}, header = u"Opacity (0.0 to 1.0)", maximum = 1.0, minimum = 0.0, value = 0.5};
auto const apply = [rectangle, box, numberOf](auto&&...) { rectangle.opacity(numberOf(box)); };
box.add_onKeyDown([apply](auto const&, KeyRoutedEventArgs& args) {
    if (args.key() == VirtualKey::Enter) {
        apply();
    }
});

auto example = rectangle;
auto options = StackPanel {box, Button {Margin {5}, hAlign.stretch, content = u"Set Opacity", onClick = apply}};