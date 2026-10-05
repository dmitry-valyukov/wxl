struct Model {
    core::observable<double> depth{32};
};
auto const model = gallery::hold<Model>();

// The grid under the card is what the shadow falls on.
auto castOnto = Grid {};
auto themeShadow = ThemeShadow {};

auto card = Border {
    width = 200,
    height = 200,
    background = brushes.Card.BackgroundFillColor.Default,
    CornerRadius {8},
    shadow = themeShadow,
    // The higher the card stands off the grid, the longer and softer the shadow.
    translation = BindOutput {model->depth, [](double height) { return Vector3 {0, 0, static_cast<float>(height)}; }},
    onLoaded = [themeShadow, castOnto](Border const&) { themeShadow.receivers().append(castOnto); },
};

auto example = Grid {Padding {36}, castOnto, card};

auto options = Slider {
    width = 200,
    hAlign.left,
    toolTip = u"shadow intensity",
    header = u"Z-translation",
    isFocusEngagementEnabled = false,
    maximum = 64.0,
    minimum = 0.0,
    smallChange = 1.0,
    stepFrequency = 1.0,
    value = Bind {model->depth},
};