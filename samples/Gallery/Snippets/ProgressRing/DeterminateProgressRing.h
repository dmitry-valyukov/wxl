// The slider writes the field, the ring shows it.
struct Model {
    core::observable<double> progress{50};
};
auto const model = gallery::hold<Model>();

auto ring = ProgressRing {
    isIndeterminate = false,
    isActive = true,
    value = BindOutput {model->progress},
    Margin {10, 10, 0, 0},
};

auto progressSlider = Slider {
    header = u"Progress",
    minimum = 0.0,
    maximum = 100.0,
    width = 200,
    value = Bind {model->progress},
};
