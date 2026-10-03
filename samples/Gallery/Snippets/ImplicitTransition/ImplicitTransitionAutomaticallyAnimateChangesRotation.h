auto const rectangle = Rectangle {
    width = 50,
    height = 50,
    Margin {45, 5, 5, 5},
    vAlign.center,
    fill = LinearGradientBrush {startPoint = Point {0.5f, 0.0f}, endPoint = Point {0.5f, 1.0f},
                                GradientStop {offset = 0.0, color = rgb(211, 211, 211)},
                                GradientStop {offset = 1.0, color = rgb(0, 120, 212)}},
    rotationTransition = ScalarTransition {},
};

auto const box = NumberBox {Margin {5}, header = u"Rotation (0.0 to 360.0)", maximum = 360.0, minimum = 0.0, value = 45.0};
auto const apply = [rectangle, box](auto&&...) {
    if (std::isnan(box.value())) {
        box.value(0.0);
    }
    // The rotation is about the middle of the rectangle.
    rectangle.centerPoint(Vector3 {static_cast<float>(rectangle.actualWidth()) / 2, static_cast<float>(rectangle.actualHeight()) / 2, 0.0f});
    rectangle.rotation(static_cast<float>(box.value()));
};
box.add_onKeyDown([apply](auto const&, KeyRoutedEventArgs& args) {
    if (args.key() == VirtualKey::Enter) {
        apply();
    }
});

auto example = rectangle;
auto options = StackPanel {box, Button {Margin {5}, hAlign.stretch, content = u"Set Rotation", onClick = apply}};