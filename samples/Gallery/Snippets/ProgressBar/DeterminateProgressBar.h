// The number box writes the field, the bar and the label show it.
struct Model {
    core::observable<double> progress{0};
};
auto const model = gallery::hold<Model>();

auto row = StackPanel {
    orientation.horizontal,
    ProgressBar {width = 130, value = BindOutput {model->progress}},
    TextBlock {width = 60, textAlignment.center, text = BindOutput {model->progress, [](double value) {
                   return core::to_u16(value, std::chars_format::fixed, 0);
               }}},
    TextBlock {u"Progress", Margin {0, 0, 10, 0}, vAlign.center},
    NumberBox {
        maximum = 100.0,
        minimum = 0.0,
        spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Inline,
        value = Bind {model->progress},
    },
};