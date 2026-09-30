// The number box writes the field, the badge shows it.
struct Model {
    core::observable<double> value{1};
};
auto const model = gallery::hold<Model>();

auto badge = InfoBadge {
    hAlign.center,
    value = BindOutput {model->value, [](double value) { return std::isnan(value) ? -1 : static_cast<int>(value); }},
};

auto valueBox = NumberBox {
    header = u"InfoBadge Value",
    minimum = -1.0,
    spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Inline,
    value = Bind {model->value},
};