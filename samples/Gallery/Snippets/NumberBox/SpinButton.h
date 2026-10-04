struct Model {
    core::observable<int> placement {1};
};
auto const model = gallery::hold<Model>();

auto number = NumberBox {
    vAlign.top,
    header = u"Enter an integer:",
    largeChange = 100.0,
    smallChange = 10.0,
    spinButtonPlacementMode = BindOutput {model->placement, [](int index) { return index == 0 ? NumberBoxSpinButtonPlacementMode::Inline : NumberBoxSpinButtonPlacementMode::Compact; }},
    value = 10.0,
};

auto placement = RadioButtons {
    header = u"SpinButton placement",
    RadioButton {content = u"Inline"},
    RadioButton {content = u"Compact"},
    selectedIndex = Bind {model->placement},
};
