auto const rectangle = Rectangle {width = 50, height = 50, Margin {45, 5, 5, 5}, vAlign.top, fill = brushes.Accent.FillColor.Default,
                                  translationTransition = Vector3Transition {}};

struct Model {
    CheckBox x {content = u"Animate X", isChecked = true};
    CheckBox y {content = u"Animate Y", isChecked = true};
    CheckBox z {content = u"Animate Z", isChecked = true};
    NumberBox custom {Margin {5}, header = u"Translation (0.0 to 200.0)", maximum = 200.0, minimum = 0.0, value = 1.0};
};
auto const model = gallery::hold<Model>();

auto const apply = [rectangle, model](float distance) {
    auto const transition = rectangle.translationTransition();
    transition.components(gallery::components(model->x.isChecked().value_or(false), model->y.isChecked().value_or(false), model->z.isChecked().value_or(false)));
    rectangle.translation(Vector3 {distance, distance, distance});
};
auto const applyCustom = [model, apply](auto&&...) {
    if (std::isnan(model->custom.value())) {
        model->custom.value(0.0);
    }
    apply(static_cast<float>(model->custom.value()));
};
model->custom.add_onKeyDown([applyCustom](auto const&, KeyRoutedEventArgs& args) {
    if (args.key() == VirtualKey::Enter) {
        applyCustom();
    }
});

auto const preset = [apply](char16_t const* title, float distance) {
    return Button {Margin {5}, hAlign.stretch, content = title, onClick = [apply, distance](auto&&...) { apply(distance); }};
};

auto example = rectangle;
auto options = StackPanel {
    preset(u"Set Translation to (0, 0, 0)", 0.0f),
    preset(u"Set Translation to (100, 100, 100)", 100.0f),
    preset(u"Set Translation to (200, 200, 200)", 200.0f),
    TextBlock {u"Components"},
    model->x,
    model->y,
    model->z,
    model->custom,
    Button {Margin {5}, hAlign.stretch, content = u"Set custom Translation", onClick = applyCustom},
};