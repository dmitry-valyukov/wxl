auto const rectangle = Rectangle {width = 50, height = 50, Margin {45, 5, 5, 5}, vAlign.top, fill = resourceBrush(u"SystemAccentColor"),
                                  scaleTransition = Vector3Transition {}};

struct Model {
    CheckBox x {content = u"Animate X", isChecked = true};
    CheckBox y {content = u"Animate Y", isChecked = true};
    CheckBox z {content = u"Animate Z", isChecked = true};
    NumberBox custom {Margin {5}, header = u"Scale (0.0 to 5.0)", maximum = 5.0, minimum = 0.0, value = 1.0};
};
auto const model = gallery::hold<Model>();

// Which components of the vector the transition animates: the others change at once.
auto const apply = [rectangle, model](float scale) {
    auto const transition = rectangle.scaleTransition();
    transition.components(gallery::components(model->x.isChecked().value_or(false), model->y.isChecked().value_or(false), model->z.isChecked().value_or(false)));
    rectangle.scale(Vector3 {scale, scale, scale});
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

auto const preset = [apply](char16_t const* title, float scale) {
    return Button {Margin {5}, hAlign.stretch, content = title, onClick = [apply, scale](auto&&...) { apply(scale); }};
};

auto example = rectangle;
auto options = StackPanel {
    preset(u"Set Scale to (0.5, 0.5, 0.5)", 0.5f),
    preset(u"Set Scale to (1.0, 1.0, 1.0)", 1.0f),
    preset(u"Set Scale to (2.0, 2.0, 2.0)", 2.0f),
    TextBlock {u"Components"},
    model->x,
    model->y,
    model->z,
    model->custom,
    Button {Margin {5}, hAlign.stretch, content = u"Set custom scale", onClick = applyCustom},
};