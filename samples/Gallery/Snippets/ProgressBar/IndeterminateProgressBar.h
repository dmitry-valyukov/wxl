// A running bar, a paused one, a failed one: one field, three radio buttons.
struct Model {
    core::observable<int> state{0};
};
auto const model = gallery::hold<Model>();

auto bar = ProgressBar {
    width = 130,
    Margin {10, 10, 0, 0},
    vAlign.top,
    isIndeterminate = true,
    showPaused = BindOutput {model->state, [](int state) { return state == 1; }},
    showError = BindOutput {model->state, [](int state) { return state == 2; }},
};

auto stateGroup = RadioButtons {
    header = u"Progress state",
    RadioButton {content = u"Running"},
    RadioButton {content = u"Paused"},
    RadioButton {content = u"Error"},
    selectedIndex = Bind {model->state},
};